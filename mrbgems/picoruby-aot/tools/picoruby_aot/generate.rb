# For one gem: decide which defs become cfuncs, emit the C, and write the
# Ruby that remains for mrbc with those defs blanked out.

require "fileutils"
require "json"
require "prism"
require "set"
require_relative "gems"
require_relative "typemap"
require_relative "rbs_index"
require_relative "codegen"

module PicorubyAot
  class Generator
    Entry = Struct.new(:file, :scope, :name, :singleton, :private, :node, :strip_node, :status, :reason, :cfunc)

    # `deps` are the PicoRuby gems this gem depends on (GemInfo, the
    # transitive closure): their defs take part in the wait search, so a
    # call into a dependency that suspends the task (JS::Promise#await in
    # picoruby-wasm) keeps the caller in the irep.
    def initialize(gem, typemap, exclude: [], include_widened: false, deps: [])
      @gem = gem
      @types = typemap
      @exclude = exclude
      @include_widened = include_widened
      @entries = []
      @aliased = []   # [name, singleton, scopes] named when the irep loads
      # Every class and module the gem and its dependencies define, with
      # or without defs: `JSON::JSONError.new` names a class of the gem,
      # whose initialize is the core's, not any gem's.
      @defined_scopes = Set.new
      # The static class hierarchy of the gem and its dependencies: the
      # superclass named by `class X < Y` (nil for a class without one,
      # :open for one whose superclass is not a constant), and the modules
      # a class body includes or prepends. A bare call resolves through it.
      @superclass = {}
      @includes = Hash.new { |h, k| h[k] = [] }
      @dep_entries = collect_dep_defs(deps)
      # The RBS of the gem and its dependencies: what a slot is declared
      # to hold, for a receiver Spinel left untyped (declared_scopes).
      @rbs = RbsIndex.new(gem.sigfiles + deps.flat_map(&:sigfiles))
    end

    attr_reader :entries, :waits

    def funcname
      @gem.name.tr("-", "_")
    end

    # Methods that hand control to the task scheduler. A cfunc cannot be
    # switched away from (mruby-task switches VM contexts between
    # instructions, never under a C frame), so a compiled loop around one
    # of these spins forever and a compiled `sleep_ms` returns at once.
    WAIT_NAMES = %w[sleep sleep_ms usleep pass suspend join wait resume].freeze
    # The C primitives of picoruby-wasm that park the task until a JS
    # promise settles are named `_*_and_suspend`; the Ruby that wraps them
    # (JS::Promise#await, JS.fetch) reaches the gem through the wait
    # search over the dependency gems.
    SUSPEND_SUFFIX = "_and_suspend"

    # Methods the VM calls by itself while the irep loads: when a class is
    # defined, a module included or extended, a method defined. A compiled
    # one is not registered yet at that moment (the same rule as a name
    # read at load), so these stay in the irep.
    HOOK_NAMES = %w[inherited included extended prepended method_added singleton_method_added].freeze

    # Calls that name methods by symbol when the irep loads. The value is
    # true when the named methods are singleton methods whatever the body,
    # nil when it depends on the body (`private :x` inside `class << self`
    # names a singleton method).
    LOAD_TIME_NAMERS = {
      private: nil, public: nil, protected: nil,
      private_class_method: true, public_class_method: true, module_function: true,
    }.freeze
    WAIT_RECEIVER_TYPES = /\Aobj_Task(__|\z)/

    # Where a call on a receiver of a core kind (Spinel's type vocabulary)
    # can land in this gem: a def that reopens that class, or one on a
    # module every object has. A call on such a receiver cannot reach a
    # def of the gem's own classes, whatever its name, so the two name
    # searches below (load-time callees, waiting callees) stop there.
    CORE_KIND_SCOPES = {
      "string" => %w[String Comparable],
      "symbol" => %w[Symbol Comparable],
      "int" => %w[Integer Numeric Comparable],
      "float" => %w[Float Numeric Comparable],
      "bool" => %w[TrueClass FalseClass],
      "nil" => %w[NilClass],
      "proc" => %w[Proc],
      "time" => %w[Time Comparable],
      "range" => %w[Range Enumerable],
      "io" => %w[IO],
      "enumerator" => %w[Enumerator Enumerable],
    }.freeze
    UNIVERSAL_SCOPES = ["", "Object", "Kernel", "BasicObject"].freeze

    # Core classes a constant may name: `Module.new`, `Struct.new`,
    # `Hash.new` reach the core, or a def of a gem that reopens the class,
    # never a gem class of another name.
    CORE_CLASS_NAMES = %w[
      BasicObject Object Module Class Kernel Comparable Enumerable
      Array Hash String Symbol Integer Float Numeric Range Proc Time
      Struct Data Exception StandardError ArgumentError RuntimeError
      TypeError NameError NoMethodError IndexError KeyError RangeError
      NilClass TrueClass FalseClass IO Enumerator
    ].freeze

    def core_scopes(raw)
      base = CORE_KIND_SCOPES[raw]
      base ||= %w[Array Enumerable] if raw.end_with?("_array")
      base ||= %w[Hash Enumerable] if raw.end_with?("_hash")
      base && base + UNIVERSAL_SCOPES
    end

    # Spinel names a class in full ("obj_Funicular__VDOM__Element") or by
    # its last constant ("obj_Renderer"); a def's scope is always full.
    def scope_is?(scope, x)
      scope == x || scope.end_with?("::#{x}")
    end

    # What a call on `recv` (in `file`, inside code whose self is a class
    # when `self_singleton`) can reach among the defs of this gem, as
    # [singleton, scopes]:
    # - singleton: true for a singleton method, false for an instance
    #   method, nil when either (an untyped receiver);
    # - scopes: the def scopes that can answer, nil when any scope can.
    # A bare call reaches self's methods or an inherited one, so only the
    # singleton-ness is known. A constant or a value Spinel typed "class"
    # is a class object, so a singleton method of a class this cannot
    # name. A core kind resolves to the defs that reopen it. An "obj_X"
    # resolves to the defs of X when the sources have any, else to the
    # hierarchy of X, or to Object's when no source defines X. A receiver
    # Spinel did not type keeps the search by name alone, unless it is a
    # local that one `new` fills (constructed_class) inside `def_node`.
    def resolve_receiver(file, recv, self_singleton, scope = nil, def_node = nil)
      # A bare call reaches self's class, its ancestors and its subclasses
      # (hierarchy_scopes), or any scope when that chain is not all in
      # the sources.
      return [self_singleton, hierarchy_scopes(scope)] if recv.nil? || recv.is_a?(Prism::SelfNode)
      raw = @types.raw_type(file, recv)
      if def_node && recv.is_a?(Prism::LocalVariableReadNode) && (raw.nil? || raw == "poly")
        x = constructed_class(recv.name, def_node, self_singleton, scope)
        return [false, [x]] if x && search_entries.any? { |o| scope_is?(o.scope, x) }
      end
      if def_node && (raw.nil? || raw == "poly")
        # Spinel holds the value as a box, but the RBS declares the slot:
        # a parameter, an instance variable, a block parameter of an
        # iteration over a declared container, or a method's value.
        declared = declared_scopes(declared_type(recv, def_node, self_singleton, scope), scope)
        return declared if declared
      end
      if (raw == "class" || raw.nil?) && (recv.is_a?(Prism::ConstantReadNode) || recv.is_a?(Prism::ConstantPathNode))
        # A constant names the class: its singleton methods, when any def
        # has that scope (an `Errors.new` is not `SQLite3::Database.new`).
        x = const_name(recv)
        return [true, [x]] if x && (CORE_CLASS_NAMES.include?(x) || @defined_scopes.any? { |s| scope_is?(s, x) } || search_entries.any? { |o| scope_is?(o.scope, x) })
        return [true, nil]
      end
      return [true, nil] if raw == "class"
      return [nil, nil] unless raw
      core = core_scopes(raw)
      return [false, core] if core
      if raw.start_with?("obj_")
        x = raw.sub("obj_", "").gsub("__", "::")
        return [false, [x]] if search_entries.any? { |o| scope_is?(o.scope, x) }
        # X has no def of its own. A class the sources define reaches its
        # hierarchy (a def it inherits, or one of a subclass). A class they
        # do not define is a C class (Littlefs::File): the Ruby methods it
        # has are reopenings of Object or Kernel.
        full = @defined_scopes.find { |s| scope_is?(s, x) }
        return [false, full ? hierarchy_scopes(full) : UNIVERSAL_SCOPES]
      end
      [nil, nil]
    end

    # The RBS type a node's value is declared to have, inside `def_node`
    # (whose scope and singleton-ness are given), or nil:
    # - a local that is a positional parameter of the def: the def's
    #   signature;
    # - a local that is a block parameter of `x.each { |e| }` and the
    #   like, where x is declared `Array[T]` or `Hash[K, V]`: T, or K and
    #   V by position;
    # - an instance variable: its declaration in the class (an attr counts);
    # - a call without arguments on a value of a declared or Spinel-typed
    #   class: that method's or attr's declared value.
    # An alias is expanded; `self` and a constant are left to the caller.
    def declared_type(node, def_node, self_singleton, scope, depth = 0)
      return nil if node.nil? || depth > 4
      case node
      when Prism::LocalVariableReadNode
        if (i = positional_param_index(def_node.parameters, node.name))
          return expand_alias(@rbs.param_type(scope, def_node.name.to_s, self_singleton, i), scope)
        end
        blk = enclosing_block_with_param(def_node, node)
        return nil unless blk
        call, position = blk
        recv_t = declared_type(call.receiver, def_node, self_singleton, scope, depth + 1)
        element_type(recv_t, call.name, position, scope)
      when Prism::InstanceVariableReadNode
        expand_alias(@rbs.ivar_type(scope, node.name.to_s), scope)
      when Prism::CallNode
        return nil if node.arguments || node.block
        owner = declared_class(node.receiver, def_node, self_singleton, scope, depth + 1)
        return nil unless owner
        expand_alias(@rbs.return_type(owner, node.name.to_s, false), scope)
      end
    end

    # The full scope name of the class a node's value is an instance of:
    # Spinel's "obj_X" when it typed it, else the declared type when that
    # names one class of the sources.
    def declared_class(node, def_node, self_singleton, scope, depth)
      return nil if node.nil?
      return (self_singleton ? nil : scope) if node.is_a?(Prism::SelfNode)
      raw = @types.raw_type(def_node_file(def_node), node)
      if raw && raw.start_with?("obj_")
        x = raw.sub("obj_", "").gsub("__", "::")
        return @defined_scopes.find { |s| scope_is?(s, x) }
      end
      t = declared_type(node, def_node, self_singleton, scope, depth)
      t = t.type if t.is_a?(RBS::Types::Optional)
      return nil unless t.is_a?(RBS::Types::ClassInstance)
      resolve_scope_name(t.name.to_s.sub(/\A::/, ""), scope)
    end

    def def_node_file(def_node)
      @def_files ||= {}.compare_by_identity
      @def_files[def_node] ||= (@entries + @dep_entries).find { |e| e.node.equal?(def_node) }&.file
    end

    # The index of `name` among the def's positional parameters (required,
    # then optional), or nil.
    def positional_param_index(params, name)
      return nil unless params
      list = params.requireds + params.optionals
      i = list.index { |p| p.respond_to?(:name) && p.name == name }
      i
    end

    # The innermost block of `def_node` whose parameters bind `name`, as
    # [the call the block is on, the parameter's position], or nil.
    def enclosing_block_with_param(def_node, read)
      found = nil
      walk = lambda do |node|
        return if found
        if node.is_a?(Prism::CallNode) && node.block.is_a?(Prism::BlockNode)
          blk = node.block
          params = blk.parameters.is_a?(Prism::BlockParametersNode) ? blk.parameters.parameters : nil
          if params
            list = params.requireds
            pos = list.index { |p| p.respond_to?(:name) && p.name == read.name }
            if pos && contains?(blk, read)
              found = [node, pos]
              walk.call(blk)   # an inner block may rebind the name
              return
            end
          end
        end
        node.compact_child_nodes.each { |c| walk.call(c) unless c.is_a?(Prism::DefNode) }
      end
      walk.call(def_node.body)
      found
    end

    def contains?(node, target)
      s = node.location.start_offset
      e = node.location.end_offset
      target.location.start_offset >= s && target.location.end_offset <= e
    end

    # What position `pos` of the block of `method` on a value of type
    # `t` receives: the element of an Array, the key or the value of a
    # Hash, the index of each_with_index.
    def element_type(t, method, pos, scope)
      return nil unless t.is_a?(RBS::Types::ClassInstance)
      name = t.name.to_s.sub(/\A::/, "")
      args = t.args
      case name
      when "Array"
        case method
        when :each, :map, :select, :reject, :find, :any?, :all?, :each_entry, :filter, :flat_map, :sum, :count, :find_index
          pos == 0 ? args[0] : nil
        when :each_with_index
          pos == 0 ? args[0] : (pos == 1 ? integer_type : nil)
        end
      when "Hash"
        case method
        when :each, :each_pair, :map, :select, :reject, :any?, :all?, :find, :filter, :count
          pos == 0 ? args[0] : (pos == 1 ? args[1] : nil)
        when :each_key then pos == 0 ? args[0] : nil
        when :each_value then pos == 0 ? args[1] : nil
        end
      end
    end

    def integer_type
      @integer_type ||= RBS::Types::ClassInstance.new(name: RBS::TypeName.parse("::Integer"), args: [], location: nil)
    end

    def expand_alias(t, scope)
      return nil if t.nil?
      if t.is_a?(RBS::Types::Alias)
        a = @rbs.alias_type(t.name.to_s.sub(/\A::/, ""), scope)
        return a ? expand_alias(a, scope) : nil
      end
      t
    end

    # Where a call on a value of declared type `t` can land, as
    # resolve_receiver answers: [singleton, scopes], or nil when the
    # declaration does not settle it (untyped, a union with an untyped
    # member, a class the sources do not define).
    DECLARED_CORE_SCOPES = {
      "Array" => %w[Array Enumerable], "Hash" => %w[Hash Enumerable],
      "String" => %w[String Comparable], "Symbol" => %w[Symbol Comparable],
      "Integer" => %w[Integer Numeric Comparable], "Float" => %w[Float Numeric Comparable],
      "Proc" => %w[Proc], "Time" => %w[Time Comparable], "Range" => %w[Range Enumerable],
      "IO" => %w[IO], "Enumerator" => %w[Enumerator Enumerable],
    }.freeze

    def declared_scopes(t, scope)
      return nil if t.nil?
      t = expand_alias(t, scope)
      case t
      when RBS::Types::Optional
        inner = declared_scopes(t.type, scope)
        inner && [inner[0], inner[1] + %w[NilClass]]
      when RBS::Types::Union
        parts = t.types.map { |u| declared_scopes(u, scope) }
        return nil if parts.any?(&:nil?)
        singleton = parts.map(&:first).uniq
        [singleton.size == 1 ? singleton[0] : nil, parts.flat_map(&:last).uniq]
      when RBS::Types::Bases::Bool then [false, %w[TrueClass FalseClass] + UNIVERSAL_SCOPES]
      when RBS::Types::Bases::Nil then [false, %w[NilClass] + UNIVERSAL_SCOPES]
      when RBS::Types::Literal
        v = t.literal
        k = v.is_a?(Symbol) ? "Symbol" : v.is_a?(String) ? "String" : v.is_a?(Integer) ? "Integer" : nil
        k && [false, DECLARED_CORE_SCOPES[k] + UNIVERSAL_SCOPES]
      when RBS::Types::Tuple then [false, DECLARED_CORE_SCOPES["Array"] + UNIVERSAL_SCOPES]
      when RBS::Types::Record then [false, DECLARED_CORE_SCOPES["Hash"] + UNIVERSAL_SCOPES]
      when RBS::Types::ClassSingleton
        x = resolve_scope_name(t.name.to_s.sub(/\A::/, ""), scope)
        x && [true, [x]]
      when RBS::Types::ClassInstance
        name = t.name.to_s.sub(/\A::/, "")
        core = DECLARED_CORE_SCOPES[name]
        return [false, core + UNIVERSAL_SCOPES] if core
        x = resolve_scope_name(name, scope)
        return nil unless x
        h = hierarchy_scopes(x)
        h && [false, h]
      end
    end

    # The nodes that bind a local: writes, destructuring targets and
    # parameters.
    LOCAL_BINDERS = [
      Prism::LocalVariableWriteNode, Prism::LocalVariableTargetNode,
      Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode,
      Prism::LocalVariableOperatorWriteNode,
      Prism::RequiredParameterNode, Prism::OptionalParameterNode,
      Prism::RestParameterNode, Prism::BlockParameterNode,
      Prism::RequiredKeywordParameterNode, Prism::OptionalKeywordParameterNode,
      Prism::KeywordRestParameterNode
    ].freeze

    # The class whose instance the local `name` holds in `def_node`, when
    # the def binds it exactly once and that write is a call of `new`:
    # the def's own class for `self.new` or a bare `new` in a singleton
    # method (`file = self.new(path)` in `File.open`), or the constant
    # receiver (`sock = TCPSocket.new(host)`). Spinel types `self` in a
    # `class << self` as a plain Class, so it gives these locals no class.
    def constructed_class(name, def_node, self_singleton, scope)
      binders = []
      local_binders(def_node, name, binders)
      return nil unless binders.size == 1 && binders[0].is_a?(Prism::LocalVariableWriteNode)
      v = binders[0].value
      return nil unless v.is_a?(Prism::CallNode) && v.name == :new
      r = v.receiver
      return (self_singleton ? scope : nil) if r.nil? || r.is_a?(Prism::SelfNode)
      const_name(r)
    end

    def local_binders(node, name, acc)
      return unless node
      acc << node if LOCAL_BINDERS.any? { |k| node.is_a?(k) } && node.name == name
      node.child_nodes.each { |c| local_binders(c, name, acc) unless c.is_a?(Prism::DefNode) }
    end

    # The defs a search runs over: this gem's, plus the dependencies' while
    # the wait search runs (see exclude_waiting_defs).
    def search_entries
      @search_entries || @entries
    end

    # Does def `e` answer a call of `name` resolved to [singleton, scopes]?
    # `X.new` runs `X#initialize`, so a singleton `new` reaches the instance
    # `initialize` of the same scopes as well.
    def answers?(e, name, singleton, scopes)
      return false unless scopes.nil? || scopes.any? { |x| scope_is?(e.scope, x) }
      return true if e.name == name && (singleton.nil? || e.singleton == singleton)
      name == "new" && singleton != false && e.name == "initialize" && !e.singleton
    end

    # Can def `e` take `argc` positional arguments? nil: not known (a
    # splat), so yes.
    def accepts?(e, argc)
      return true if argc.nil?
      p = e.node.parameters
      return argc == 0 if p.nil?
      req = p.requireds.size + p.posts.size
      return false if argc < req
      p.rest ? true : argc <= req + p.optionals.size
    end

    # The positional arguments a call passes; nil when a splat hides it.
    def call_argc(call)
      return 0 unless call.arguments
      args = call.arguments.arguments
      return nil if args.any? { |a| a.is_a?(Prism::SplatNode) }
      args.count { |a| !a.is_a?(Prism::BlockArgumentNode) && !a.is_a?(Prism::KeywordHashNode) }
    end

    # The defs a call of `name` with `argc` positional arguments can
    # reach, given resolve_receiver's answer. A def that cannot take the
    # arguments is not the one called (`parts.join(", ")` is never a
    # `def join` without parameters).
    def reachable_defs(name, singleton, scopes, argc = nil)
      pool = search_entries
      if @by_name
        pool = @by_name[name]
        pool += @by_name["initialize"] if name == "new" && singleton != false
      end
      pool.select { |o| answers?(o, name, singleton, scopes) && accepts?(o, argc) }
    end

    # A block whose body runs later, when the method it defines or the
    # Proc it builds is called, not when the irep loads.
    DEFERRED_BLOCK_CALLS = %i[define_method define_singleton_method lambda proc new].freeze

    def deferred_block?(call)
      return false unless call.block.is_a?(Prism::BlockNode)
      return false unless DEFERRED_BLOCK_CALLS.include?(call.name)
      return true unless call.name == :new
      call.receiver.is_a?(Prism::ConstantReadNode) && call.receiver.name == :Proc
    end

    # Walk every mrblib file and classify every def.
    def collect
      @gem.rbfiles.each do |file|
        src = File.read(file)
        result = Prism.parse(src, filepath: file)
        raise "parse error in #{file}" if result.failure?
        walk_body(result.value.statements, [], file, false)
      end
      exclude_aliased_defs
      exclude_waiting_defs
      self
    end

    # A def waits if it calls a scheduler method, anything on a Task, or
    # (transitively) another def of this gem that waits. Receivers are
    # resolved through Spinel's type when it names a class of this gem,
    # and a receiver of a core kind (`list.join`, `name.to_s`) is a call
    # into the core, not a Task#join nor a waiting def of the gem.
    def exclude_waiting_defs
      # The defs of this gem and of its dependencies: a wait in a
      # dependency (JS::Promise#await calls _await_and_suspend) reaches
      # this gem's defs through the same search by name.
      all = @entries + @dep_entries
      @search_entries = all
      by_label = {}
      all.reverse_each { |e| by_label["#{e.scope}#{e.singleton ? '.' : '#'}#{e.name}"] = e }
      # Entry is a Struct whose hash follows its fields: key by identity.
      # @waits keeps the result (def -> the call that made it wait) for
      # the tools that explain a decision.
      @waits = waits = {}.compare_by_identity
      calls = {}.compare_by_identity
      all.each do |e|
        calls[e] = []
        collect_calls(e.node.body, calls[e])
      end
      # Spinel typed nothing in a dependency, so a search by name through
      # the dependencies' untyped receivers would meet (`x.size`,
      # `y.to_s`) and make every def wait. Inside a dependency the search
      # follows only what it can resolve: a bare call to the same scope,
      # a call on a constant to that constant's singleton methods, and a
      # call by the name of a def that reaches a primitive directly
      # (`await`, `fetch`, `getch`). This gem's defs search by name as
      # before, over its own defs and the dependencies'.
      wrappers = @dep_entries.select { |e| calls[e].any? { |c| direct_wait?(e, c) } }
      # Each call is resolved once, to :direct or to the defs whose wait it
      # inherits; the fixpoint below then reads only `waits`. (Resolving
      # inside the fixpoint cost 14 s on funicular.)
      @by_name = Hash.new { |h, k| h[k] = [] }
      all.each { |o| @by_name[o.name] << o }
      resolved = {}.compare_by_identity
      all.each do |e|
        resolved[e] = calls[e].map do |call|
          next [call, :direct] if direct_wait?(e, call)
          name = call.name.to_s
          recv = call.receiver
          argc = call_argc(call)
          # A bare call to a def of this class goes there and nowhere else.
          if recv.nil? || recv.is_a?(Prism::SelfNode)
            target = by_label["#{e.scope}#{e.singleton ? '.' : '#'}#{name}"]
            next [call, [target]] if target
          end
          if e.status == :foreign
            if recv.is_a?(Prism::ConstantReadNode) || recv.is_a?(Prism::ConstantPathNode)
              next [call, reachable_defs(name, true, [const_name(recv)], argc)]
            end
            next [call, wrappers.select { |o| o.name == name && accepts?(o, argc) }]
          end
          # Otherwise every def the call can reach: if one waits, assume
          # this call does too.
          singleton, scopes = resolve_receiver(e.file, recv, e.singleton, e.scope, e.node)
          [call, reachable_defs(name, singleton, scopes, argc)]
        end
      end
      loop do
        changed = false
        all.each do |e|
          next if waits[e]
          hit = resolved[e].find { |_, deps| deps == :direct || deps.any? { |o| waits[o] } }
          next unless hit
          waits[e] = hit[0]
          changed = true
        end
        break unless changed
      end
      @by_name = nil
      @search_entries = nil
      waits.each do |e, call|
        next unless e.status == :candidate
        e.status = :irep
        e.reason = "waits for the scheduler (#{call.name} at line #{call.location.start_line}); a cfunc cannot yield to other tasks"
      end
    end

    def collect_calls(node, acc)
      return unless node
      acc << node if node.is_a?(Prism::CallNode)
      node.child_nodes.each { |c| collect_calls(c, acc) }
    end

    # Does `call`, in def `e`, hand control to the scheduler by itself: a
    # wasm primitive that suspends, anything on a Task, or a scheduler
    # method by name on a bare call or on a value whose class is not known
    # (a Task is one of the possibilities).
    def direct_wait?(e, call)
      name = call.name.to_s
      recv = call.receiver
      return true if name.end_with?(SUSPEND_SUFFIX)
      return true if recv.is_a?(Prism::ConstantReadNode) && recv.name == :Task
      raw = @types.raw_type(e.file, recv) if recv
      return true if raw && raw =~ WAIT_RECEIVER_TYPES
      return false unless WAIT_NAMES.include?(name)
      # Task#join takes no argument: `parts.join(", ")` is Array#join.
      return false if name == "join" && call.arguments && !call.arguments.arguments.empty?
      # A bare `sleep_ms` is the Kernel's.
      return true if recv.nil? || recv.is_a?(Prism::SelfNode)
      return false unless recv.is_a?(Prism::ConstantReadNode) || recv.is_a?(Prism::ConstantPathNode) || recv.is_a?(Prism::CallNode)
      _, scopes = resolve_receiver(e.file, recv, e.singleton, e.scope, e.node)
      scopes.nil?
    end

    # The defs of the dependency gems, for the wait search only: name,
    # scope, singleton-ness and body. They are never candidates
    # (status :foreign), and Spinel typed none of their receivers, so the
    # search through them goes by name.
    def collect_dep_defs(deps)
      entries = []
      deps.each do |g|
        g.rbfiles.each do |file|
          result = Prism.parse(File.read(file), filepath: file)
          next if result.failure?
          collect_dep_defs_in(result.value.statements, [], file, false, entries)
        end
      end
      entries
    end

    def collect_dep_defs_in(node, scope, file, singleton, entries)
      return if node.nil?
      case node
      when Prism::ClassNode, Prism::ModuleNode
        name = const_name(node.constant_path)
        inner = name.start_with?("::") ? [name.sub("::", "")] : scope + [name]
        @defined_scopes << inner.join("::")
        record_superclass(node, inner.join("::"))
        collect_dep_defs_in(node.body, inner, file, false, entries)
      when Prism::SingletonClassNode
        collect_dep_defs_in(node.body, scope, file, true, entries) if node.expression.is_a?(Prism::SelfNode)
      when Prism::DefNode
        s = singleton || node.receiver.is_a?(Prism::SelfNode)
        entries << Entry.new(file, scope.join("::"), node.name.to_s, s, false, node, nil, :foreign, nil, nil)
      else
        record_include(node, scope.join("::")) if node.is_a?(Prism::CallNode)
        node.compact_child_nodes.each { |c| collect_dep_defs_in(c, scope, file, singleton, entries) }
      end
    end

    # `class X < Y`: Y by name, nil without one, :open when it is not a
    # constant (`Struct.new(...)`, a variable).
    def record_superclass(node, scope)
      return unless node.is_a?(Prism::ClassNode)
      sup = node.superclass
      return if sup.nil?
      @superclass[scope] = (sup.is_a?(Prism::ConstantReadNode) || sup.is_a?(Prism::ConstantPathNode)) ? const_name(sup) : :open
    end

    # `include M` / `prepend M` at class-body level, with constant names.
    def record_include(node, scope)
      return unless node.receiver.nil? && %i[include prepend].include?(node.name)
      args = node.arguments ? node.arguments.arguments : []
      return if args.empty?
      args.each do |a|
        if a.is_a?(Prism::ConstantReadNode) || a.is_a?(Prism::ConstantPathNode)
          @includes[scope] << const_name(a)
        else
          @includes[scope] << :open
        end
      end
    end

    # The scope `name` refers to, seen from `from`: lexical nesting first
    # (`Base` inside `Editor::Screen` is `Editor::Base`), then any defined
    # scope that ends with the name. nil when none is defined (a core
    # class, a class of a gem outside the build, a C-defined class).
    def resolve_scope_name(name, from)
      parts = from.split("::")
      while true
        candidate = (parts + [name]).join("::")
        return candidate if @defined_scopes.include?(candidate)
        break if parts.empty?
        parts.pop
      end
      @defined_scopes.find { |s| scope_is?(s, name) }
    end

    # The scopes a bare call inside `scope` can reach: the class itself, its
    # ancestors through `class X < Y` and `include`, and its descendants (a
    # subclass may override the method), plus the modules every object has.
    # nil when the chain leaves what the sources define through an
    # unresolved superclass or include (:open), because any def of the
    # name may then answer.
    def hierarchy_scopes(scope)
      return nil if scope.nil?
      @hierarchy ||= {}
      return @hierarchy[scope] if @hierarchy.key?(scope)
      @hierarchy[scope] = compute_hierarchy_scopes(scope)
    end

    def compute_hierarchy_scopes(scope)
      ancestors = []
      queue = [scope]
      while (s = queue.shift)
        next if ancestors.include?(s)
        ancestors << s
        sup = @superclass[s]
        return nil if sup == :open
        if sup
          # A superclass the sources do not define (StandardError, a
          # C-defined class) adds no gem def: the walk stops there.
          r = resolve_scope_name(sup, s)
          queue << r if r
        end
        @includes[s].each do |m|
          return nil if m == :open
          r = resolve_scope_name(m, s)
          queue << r if r
        end
      end
      descendants = @defined_scopes.select do |d|
        chain = [d]
        seen = []
        hit = false
        while (c = chain.shift)
          next if seen.include?(c)
          seen << c
          hit = true if ancestors.include?(c) && c != d
          sup = @superclass[c]
          chain << resolve_scope_name(sup, c) if sup && sup != :open
          @includes[c].each { |m| chain << resolve_scope_name(m, c) if m != :open }
          chain.compact!
        end
        hit && !ancestors.include?(d)
      end
      ancestors + descendants + UNIVERSAL_SCOPES
    end

    # The label Spinel's codegen uses for a def: "Point#dist2", "Point.make",
    # or the bare name of a top-level def.
    def spinel_label(e)
      return e.name if e.scope.empty?
      "#{e.scope}#{e.singleton ? '.' : '#'}#{e.name}"
    end

    def compile
      used = {}
      targets = {}
      @entries.each do |e|
        next unless e.status == :candidate
        base = "aot_#{e.scope.tr(':', '_')}_#{e.singleton ? 'S_' : ''}#{e.name}".gsub(/[^A-Za-z0-9_]/) { |c| "_%02x" % c.ord }
        n = (used[base] = (used[base] || 0) + 1)
        e.cfunc = n == 1 ? base : "#{base}_#{n}"
        begin
          probe = Compiler.new(@types, e.file, e.node, scope: e.scope, cfunc_name: e.cfunc, singleton: e.singleton)
          kinds = probe.param_kinds
          required = probe.required_count
        rescue Unsupported, StandardError
          next # the def will not compile; no one calls it directly
        end
        targets[spinel_label(e)] = { cfunc: e.cfunc, kinds: kinds, required: required, kw: probe.kw_params, block: probe.block?, private: e.private }
      end
      # A direct call needs its target compiled. Compile everything against
      # the optimistic target set; when a target drops out, compile again
      # without it, until the set is stable (a few rounds at most).
      loop do
        @c_functions = []
        @prototypes = []
        @syms = {}
        @paths = {}
        @registrations = []
        dropped = false
        @entries.each do |e|
          next unless e.status == :candidate || e.status == :aot
          compiler = Compiler.new(@types, e.file, e.node, scope: e.scope, cfunc_name: e.cfunc, singleton: e.singleton, syms: @syms, paths: @paths, targets: targets)
          begin
            code = compiler.compile
          rescue Unsupported => ex
            e.status = :irep
            e.reason = ex.message
            dropped |= !targets.delete(spinel_label(e)).nil?
            next
          rescue StandardError => ex
            # A generator bug must never break the build: the def stays Ruby.
            e.status = :irep
            e.reason = "generator error: #{ex.class}: #{ex.message} (#{ex.backtrace&.first})"
            dropped |= !targets.delete(spinel_label(e)).nil?
            next
          end
          e.status = :aot
          @c_functions << code
          @prototypes << compiler.impl_prototype
          # `initialize` is private wherever it is defined, as `def initialize`
          # in the irep makes it; `Class#new` reaches it all the same.
          private_def = e.private || (e.name == "initialize" && !e.singleton)
          define = private_def ? "mrb_define_private_method" : (e.singleton ? "mrb_define_class_method" : "mrb_define_method")
          # aot_scope_init records the scope's classes for the body's constant
          # reads (a path is initialized once; later calls only re-read it).
          target = e.scope.empty? ? "mrb->object_class" : "aot_scope_init(mrb, &#{Compiler.path_name(@paths, e.scope)})"
          # A private singleton method (`private` inside `class << self`) is
          # a private method of the singleton class.
          target = "mrb_singleton_class_ptr(mrb, mrb_obj_value(#{target}))" if e.singleton && e.private
          @registrations << "  #{define}(mrb, #{target}, #{c_str(e.name)}, #{e.cfunc}, #{compiler.arity_spec});"
        end
        break unless dropped
      end
      self
    end

    def c_source
      s = +"/* Generated by picoruby-aot for #{@gem.name}. Do not edit. */\n"
      s << "#include <mruby.h>\n#include <mruby/proc.h>\n#include <math.h>\n#include \"picoruby_aot.h\"\n\n"
      decls = Compiler.sym_decls(@syms) + Compiler.path_decls(@paths)
      s << decls << "\n" unless decls.empty?
      s << @prototypes.join << "\n" unless @prototypes.empty?
      s << @c_functions.join("\n")
      s << "\nvoid\nmrb_aot_#{funcname}_register(mrb_state *mrb)\n{\n"
      s << @registrations.join("\n") << "\n" unless @registrations.empty?
      s << "  (void)mrb;\n" if @registrations.empty?
      s << "}\n"
      s
    end

    # The Ruby left for mrbc: each compiled def replaced by spaces, so that
    # line numbers in the rest of the file do not move.
    def stripped_sources
      @gem.rbfiles.map do |file|
        src = File.read(file).b
        @entries.select { |e| e.file == file && e.status == :aot }.each do |e|
          loc = e.strip_node.location
          blank = src[loc.start_offset, loc.length].gsub(/[^\n]/, " ")
          src[loc.start_offset, loc.length] = blank
        end
        [file, src.force_encoding("UTF-8")]
      end
    end

    def manifest
      {
        gem: @gem.name,
        defs: @entries.map { |e|
          { file: e.file.sub(@gem.dir + "/", ""), line: e.node.location.start_line,
            method: "#{e.scope}#{e.singleton ? '.' : '#'}#{e.name}",
            status: e.status.to_s, reason: e.reason }.compact
        },
      }
    end

    private

    def c_str(s)
      '"' + s.gsub('\\', '\\\\\\\\').gsub('"', '\\"') + '"'
    end

    def const_name(node)
      case node
      when Prism::ConstantReadNode then node.name.to_s
      when Prism::ConstantPathNode
        parent = node.parent ? const_name(node.parent) : nil
        parent ? "#{parent}::#{node.name}" : node.name.to_s
      end
    end

    # `singleton` is true inside `class << self`: every def there is a
    # singleton method of the enclosing class, and `private` applies to
    # them (unlike `def self.x`, which `private` never touches).
    def walk_body(stmts, scope, file, private_default, singleton: false)
      return unless stmts
      list = stmts.is_a?(Prism::StatementsNode) ? stmts.body : [stmts]
      private_now = private_default
      list.each do |node|
        case node
        when Prism::ClassNode, Prism::ModuleNode
          name = const_name(node.constant_path)
          inner = name.start_with?("::") ? [name.sub("::", "")] : scope + [name]
          @defined_scopes << inner.join("::")
          record_superclass(node, inner.join("::"))
          walk_body(node.body, inner, file, false)
        when Prism::SingletonClassNode
          if node.expression.is_a?(Prism::SelfNode) && !scope.empty? && !singleton
            walk_body(node.body, scope, file, false, singleton: true)
          end
        when Prism::DefNode
          add_def(node, node, scope, file, private_now, singleton)
        when Prism::AliasMethodNode
          @aliased << [node.old_name.unescaped, singleton, nil] if node.old_name.is_a?(Prism::SymbolNode)
        when Prism::CallNode
          args = node.arguments ? node.arguments.arguments : []
          if node.receiver.nil? && args.empty? && %i[private public protected].include?(node.name)
            private_now = (node.name == :private)
          elsif node.receiver.nil? && %i[private public].include?(node.name) &&
                args.size == 1 && args[0].is_a?(Prism::DefNode)
            add_def(args[0], node, scope, file, node.name == :private, singleton)
          elsif node.receiver.nil? && node.name == :alias_method && args.size == 2 && args[1].is_a?(Prism::SymbolNode)
            @aliased << [args[1].unescaped, singleton, nil]
          elsif node.receiver.nil? && LOAD_TIME_NAMERS.key?(node.name) && !args.empty? && args.all? { |a| a.is_a?(Prism::SymbolNode) }
            # `private :a, :b` and the like look the methods up by name
            # when the irep loads, as an alias does.
            on_singleton = LOAD_TIME_NAMERS[node.name] || singleton
            args.each { |a| @aliased << [a.unescaped, on_singleton, nil] }
          else
            record_include(node, scope.join("::"))
            scan_load_time(node, singleton, file, scope.empty?, scope.join("::"))
          end
        else
          # A def under `if`/`begin`/... stays in the irep (it is defined
          # conditionally); everything else at this level runs when the
          # irep loads.
          scan_load_time(node, singleton, file, scope.empty?, scope.join("::"))
        end
      end
    end

    # Code at class-body level runs when the irep loads, before the cfuncs
    # are registered. A method it calls then (a DSL such as
    # `define_voice_program :kick, [...]`, a constant built by a call) must
    # still be in the irep, and so must one an alias or a visibility call
    # names. Defs under such code are not candidates (see walk_body).
    # `singleton` is the `class << self` flag of walk_body (what `private
    # :x` names); `top_level` says whether self is the main object rather
    # than the class being defined (what a bare call reaches).
    def scan_load_time(node, singleton, file, top_level, scope)
      return if node.nil?
      case node
      when Prism::DefNode, Prism::ClassNode, Prism::ModuleNode, Prism::SingletonClassNode
        return
      when Prism::AliasMethodNode
        @aliased << [node.old_name.unescaped, singleton, nil] if node.old_name.is_a?(Prism::SymbolNode)
      when Prism::CallNode
        args = node.arguments ? node.arguments.arguments : []
        if node.receiver.nil? && node.name == :alias_method && args.size == 2 && args[1].is_a?(Prism::SymbolNode)
          @aliased << [args[1].unescaped, singleton, nil]
        elsif node.receiver.nil? && LOAD_TIME_NAMERS.key?(node.name) && !args.empty? && args.all? { |a| a.is_a?(Prism::SymbolNode) }
          on_singleton = LOAD_TIME_NAMERS[node.name] || singleton
          args.each { |a| @aliased << [a.unescaped, on_singleton, nil] }
        else
          s, scopes = resolve_receiver(file, node.receiver, top_level ? nil : true, scope)
          @aliased << [node.name.to_s, s, scopes, call_argc(node)]
        end
        # The body of a define_method or a Proc runs later, not now.
        if deferred_block?(node)
          node.compact_child_nodes.each { |c| scan_load_time(c, singleton, file, top_level, scope) unless c.equal?(node.block) }
          return
        end
      when Prism::LambdaNode
        return
      end
      node.compact_child_nodes.each { |c| scan_load_time(c, singleton, file, top_level, scope) }
    end

    # `alias`, `alias_method`, `private :x` and the like run when the irep
    # loads, before the cfuncs are registered, and look the method up by
    # name then. A def whose name such a call of the gem reads (in any
    # scope: the call may sit in a subclass) stays in the irep, and so does
    # a later def of that name.
    #
    # An entry of `named` is [name, singleton, scopes] as resolve_receiver
    # answers it: `singleton` nil when either kind of method can answer,
    # `scopes` nil when any scope can.
    def exclude_aliased_defs
      # Transitively: a method the load-time code calls may call others of
      # the gem (`define_voice_program` calls `voice_program`), and those
      # run before registration too. Each call is resolved as far as
      # Spinel's type of its receiver allows; a call on an untyped value
      # takes every def of its name.
      named = @aliased.uniq
      named_match = lambda do |e|
        named.any? { |n, s, sc, argc| answers?(e, n, s, sc) && accepts?(e, argc) }
      end
      seen = {}.compare_by_identity
      loop do
        added = false
        @entries.each do |e|
          next if seen[e] || !named_match.call(e)
          seen[e] = true
          calls = []
          collect_calls(e.node.body, calls)
          calls.each do |c|
            s, scopes = resolve_receiver(e.file, c.receiver, e.singleton, e.scope, e.node)
            k = [c.name.to_s, s, scopes, call_argc(c)]
            next if named.include?(k)
            named << k
            added = true
          end
        end
        break unless added
      end
      @entries.each do |e|
        next unless e.status == :candidate
        next unless named_match.call(e)
        e.status = :irep
        e.reason = "named or called when the irep loads (class-body code, alias, alias_method, private :x, or a method they call), before the cfunc is registered"
      end
    end

    def add_def(def_node, strip_node, scope, file, is_private, singleton_body = false)
      singleton = singleton_body
      if def_node.receiver
        # `def self.x` inside `class << self` is a method of the singleton's
        # singleton: left alone. Elsewhere it is a singleton method, and a
        # `private` above it does not apply to it.
        return if singleton_body
        return unless def_node.receiver.is_a?(Prism::SelfNode)
        singleton = true
        is_private = false
      end
      e = Entry.new(file, scope.join("::"), def_node.name.to_s, singleton, is_private, def_node, strip_node, :candidate, nil, nil)
      @entries << e
      label = "#{e.scope}#{singleton ? '.' : '#'}#{e.name}"
      rec = @types.def_record(file, def_node)
      if @exclude.include?(label)
        e.status = :irep
        e.reason = "excluded by spec.aot_exclude"
      elsif HOOK_NAMES.include?(e.name)
        e.status = :irep
        e.reason = "a hook the VM calls while the irep loads (#{e.name}), before the cfunc is registered"
      elsif rec.nil?
        e.status = :irep
        e.reason = "no type record from spinel"
      elsif rec["widened"] && !@include_widened
        e.status = :irep
        e.reason = "widened to untyped: #{rec['signature']}"
      end
    end
  end
end
