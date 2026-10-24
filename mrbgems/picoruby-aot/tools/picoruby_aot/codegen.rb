# Prism AST + Spinel types -> C that calls the mruby API.
#
# One Compiler instance compiles one `def`.
#
# Value kinds: :int (aot_int, 64 bits on every target), :float
# (mrb_float), :bool (mrb_bool), :nil
# (no storage) and :value (mrb_value). A value is unboxed when the
# generated C itself produced it (a literal, arithmetic on unboxed
# operands) or when Spinel proved its kind and that no nil reaches it
# (`--emit-types` "nullable":false; an Integer and an Integer? share the
# tag "int"):
#
# - a parameter so proved is read boxed and unboxed by a checked helper
#   that takes the proved class and nothing else (a Float for an Integer
#   raises TypeError, as does an Integer for a Float or a non-boolean for
#   a bool), unless the body tests the parameter's class itself, which
#   says the author expects callers outside the analysis;
# - an instance variable read or a call result so proved is unboxed with
#   a checked helper: a value the analysis did not predict (a method
#   redefined at runtime) raises TypeError instead of being reinterpreted.
#
# Everything else that arrives from the VM stays an mrb_value, and
# arithmetic on it goes through guarded helpers: an Integer/Float fast path
# inline, and mrb_funcall for anything else (nil, a Bignum, a user-defined
# operator). Anything the generator does not handle raises Unsupported and
# the def stays in the irep.

require "prism"
require_relative "typemap"

module PicorubyAot
  class Unsupported < StandardError; end

  # `proved` is :int or :float on a boxed value (kind :value) that Spinel
  # proved a non-nil scalar: it is unboxed, with a check, at the first use
  # that needs the C scalar, and never when it is only discarded, returned,
  # tested for truth or passed on.
  Val = Struct.new(:code, :kind, :proved)

  class Compiler
    CTYPE = { int: "aot_int", float: "mrb_float", bool: "mrb_bool", value: "mrb_value" }.freeze

    # Integer literals the generated C can carry: the 64-bit slot (aot_int)
    # on every target. A literal past mrb_int is boxed as a Bignum when the
    # value reaches the VM.
    INT_LIT_MAX = 2**63 - 1

    INT_BINOPS = {
      "+" => "aot_int_add", "-" => "aot_int_sub", "*" => "aot_int_mul",
      "/" => "aot_int_div", "%" => "aot_int_mod", "**" => "aot_int_pow",
      "<<" => "aot_int_lshift", ">>" => "aot_int_rshift",
    }.freeze
    INT_BITOPS = { "&" => "&", "|" => "|", "^" => "^" }.freeze
    CMP_OPS = { "<" => "<", "<=" => "<=", ">" => ">", ">=" => ">=", "==" => "==", "!=" => "!=" }.freeze
    FLOAT_BINOPS = { "+" => "+", "-" => "-", "*" => "*", "/" => "/" }.freeze

    # String methods with a guarded helper (picoruby_aot.h), emitted where
    # Spinel typed the receiver a String; keyed by name and argument count.
    STR_METHODS = {
      ["size", 0] => "aot_str_size", ["length", 0] => "aot_str_length",
      ["bytesize", 0] => "aot_str_bytesize", ["empty?", 0] => "aot_str_empty_p",
      ["getbyte", 1] => "aot_str_getbyte", ["[]", 1] => "aot_str_aref1",
      ["[]", 2] => "aot_str_aref2", ["byteslice", 2] => "aot_str_byteslice",
    }.freeze

    # Likewise for an Array (Spinel type "*_array") and a Hash ("*_hash").
    ARY_METHODS = {
      ["[]", 1] => "aot_ary_aref", ["[]=", 2] => "aot_ary_aset",
      ["size", 0] => "aot_ary_size", ["length", 0] => "aot_ary_length",
      ["empty?", 0] => "aot_ary_empty_p", ["push", 1] => "aot_ary_push",
      ["pop", 0] => "aot_ary_pop", ["shift", 0] => "aot_ary_shift",
      ["first", 0] => "aot_ary_first", ["last", 0] => "aot_ary_last",
      ["clear", 0] => "aot_ary_clear",
    }.freeze
    HASH_METHODS = {
      ["[]", 1] => "aot_hash_aref", ["[]=", 2] => "aot_hash_aset",
      ["key?", 1] => "aot_hash_key_p", ["has_key?", 1] => "aot_hash_key_p",
      ["include?", 1] => "aot_hash_key_p", ["size", 0] => "aot_hash_size",
      ["length", 0] => "aot_hash_size", ["delete", 1] => "aot_hash_delete",
    }.freeze

    # Core methods on a receiver Spinel left untyped (a widened def's
    # values): the helper tests the class and the method at run time and
    # sends to the VM when the receiver is anything else. Emitted only
    # where no compiled def of this gem is the direct target.
    GENERIC_METHODS = {
      ["[]", 1] => "aot_aref", ["[]=", 2] => "aot_aset",
      ["size", 0] => "aot_size", ["length", 0] => "aot_length",
      ["empty?", 0] => "aot_empty_p", ["include?", 1] => "aot_include_p",
      ["key?", 1] => "aot_hash_key_p", ["has_key?", 1] => "aot_hash_key_p",
      ["keys", 0] => "aot_keys", ["values", 0] => "aot_values",
      ["first", 0] => "aot_first", ["last", 0] => "aot_last",
      ["push", 1] => "aot_ary_push", ["pop", 0] => "aot_ary_pop", ["shift", 0] => "aot_ary_shift",
      ["nil?", 0] => "aot_nil_p", ["is_a?", 1] => "aot_is_a_p", ["kind_of?", 1] => "aot_kind_of_p",
      ["to_s", 0] => "aot_to_s", ["to_sym", 0] => "aot_to_sym",
    }.freeze

    # Iterations with a block literal on an untyped receiver: a C loop
    # over an Array that yields to the block, else the VM.
    LOOP_METHODS = {
      "each" => "aot_each", "each_pair" => "aot_each_pair", "each_with_index" => "aot_each_with_index",
      "map" => "aot_map", "select" => "aot_select", "any?" => "aot_any_p", "all?" => "aot_all_p",
      "times" => "aot_times",
    }.freeze

    # Boxed binary operators with a guarded helper (picoruby_aot.h).
    GUARDED = {
      "+" => "aot_add", "-" => "aot_sub", "*" => "aot_mul", "/" => "aot_div", "%" => "aot_mod",
      "<" => "aot_lt", "<=" => "aot_le", ">" => "aot_gt", ">=" => "aot_ge",
      "==" => "aot_eq", "!=" => "aot_neq",
      "&" => "aot_band", "|" => "aot_bor", "^" => "aot_bxor", "<<" => "aot_lshift", ">>" => "aot_rshift",
    }.freeze

    # The block a cfunc received, as the impl's last C argument. A def with
    # `&blk` names it; a def that uses `yield` or `block_given?` has it as a
    # hidden parameter. nil when the caller gave no block.
    BLOCK_C = "p_blk"

    def initialize(typemap, file, def_node, scope:, cfunc_name:, singleton: false, syms: {}, paths: {}, consts: {}, targets: {})
      @types = typemap
      @file = file
      @def = def_node
      @scope = scope
      @singleton = singleton   # a singleton method: `self` is the class, a self call lands on "Scope.name"
      @cfunc = cfunc_name
      @tmp = 0
      @locals = {}       # ruby name -> [c name, kind] ([c name, :value, :cell] for a cell local)
      @cells = {}        # ruby name -> true for a local a block shares with the method (collect_cells)
      @syms = syms       # ruby name -> c static name, shared by the translation unit
      @paths = paths     # "A::B" -> c static name of its aot_path, shared likewise
      @consts = consts   # [scope, "A::B"] -> c static name of its aot_cslot, shared likewise
      # Spinel callee label ("Point#dist2") -> { cfunc:, kinds:, private: }
      # for the defs of this gem that compile; a direct call goes to one.
      @targets = targets
      @direct_refs = []  # the labels this def calls directly
      @loop_depth = 0
      @loops = []        # the while/until nodes around the code being emitted
      @block_functions = [] # the C of each block literal's cfunc, in order
      @in_block = false  # emitting a block literal's body
      @inline = nil      # the inline loop being emitted (inline_loop): its next label and value
      @inline_seq = 0
      @begins = 0        # begin blocks so far (names their C functions)
      @regions = {}.compare_by_identity # BeginNode -> the locals its body and its clauses own
      @ud = nil          # the userdata C variable while emitting a begin's function
      @private = {}      # the locals the begin region being emitted owns
      @outer_loops = 0   # loops around the begin whose function is being emitted
      @jumps = {}        # the jumps the current begin's functions record
    end

    attr_reader :direct_refs

    # The kinds the parameters take, as the impl's C signature has them;
    # also what a direct caller passes. Decided from the types alone, so a
    # caller can know them before the def is compiled.
    # The positional parameters' kinds and required count (the keyword
    # parameters are kw_params).
    def param_kinds
      check_params unless @params
      @params.reject { |p| p[:kw] || p[:block] }.map { |p| p[:kind] }
    end

    def required_count
      check_params unless @params
      @params.count { |p| !p[:optional] && !p[:kw] && !p[:block] }
    end

    # Does the impl take the block as its last argument? A direct caller
    # then passes the block it forwards, or nil.
    def block?
      check_params unless @params
      @params.any? { |p| p[:block] }
    end

    # The keyword parameters as a direct caller needs them: name, kind and
    # whether the keyword is required, in impl order.
    def kw_params
      check_params unless @params
      @params.select { |p| p[:kw] }.map { |p| { name: p[:name], kind: p[:kind], required: !p[:optional] } }
    end

    def impl_name
      "#{@cfunc}_impl"
    end

    # Both functions, since a direct call site names the cfunc (for the
    # method test) and the impl before either is defined.
    def impl_prototype
      "static mrb_value #{impl_name}(mrb_state *mrb, mrb_value self#{impl_params});\n" \
      "static mrb_value #{@cfunc}(mrb_state *mrb, mrb_value self);\n"
    end

    def impl_params
      check_params unless @params
      @params.map { |p| ", #{CTYPE[p[:kind]]} #{p[:c]}" }.join
    end

    # The static `struct aot_path` for a constant path ("A::B::C"): the
    # names once, the symbols interned on first use, and for a method's
    # lexical scope the classes recorded at registration.
    def self.path_name(paths, path)
      paths[path] ||= "path_#{paths.size}"
    end

    def self.path_decls(paths)
      paths.map do |path, c|
        parts = path.split("::")
        names = parts.map { |s| '"' + s + '"' }.join(", ")
        "static const char *const #{c}_names[] = { #{names} };\n" \
        "static mrb_sym #{c}_syms[#{parts.size}];\n" \
        "static struct RClass *#{c}_klass[#{parts.size}];\n" \
        "static struct aot_path #{c} = { #{parts.size}, #{c}_names, #{c}_syms, #{c}_klass };\n"
      end.join
    end

    # Two C functions per def: the impl, which takes self and the
    # parameters as C arguments (a direct call from another compiled def
    # lands here, with no VM frame), and the mruby cfunc, which reads the
    # arguments with mrb_get_args and calls the impl.
    def compile
      check_params unless @params
      collect_cells(@def.body)
      collect_begin_cells(@def.body)
      infer_local_kinds(@def.body)
      body = +""
      @out = body
      emit_defaults
      enter_cells
      if @def.body
        last = statements(@def.body)
        emit "return #{box(last)};"
      else
        emit "return mrb_nil_value();"
      end
      param_names = @params.map { |p| p[:c] }
      dead = drop_unused_temps(body, param_names)
      # The block cfuncs first: the impl names them.
      s = @block_functions.join
      s << "static mrb_value\n#{impl_name}(mrb_state *mrb, mrb_value self#{impl_params})\n{\n"
      @locals.each_value do |c, kind|
        next if kind == :nil || param_names.include?(c) || dead.include?(c)
        init = kind == :value ? " = mrb_nil_value()" : " = 0"
        s << "  #{CTYPE[kind]} #{c}#{init};\n"
      end
      # A direct call runs on the machine stack with no VM entry, so the
      # VM's own check (mrb_funcall, mrb_yield, mrb_vm_run) never sees a
      # recursion through compiled methods. The impl makes the check
      # itself: with MRB_USE_C_STACK_CHECK a recursion that reaches the
      # host's floor raises SystemStackError, without the macro this is
      # nothing.
      s << "  mrb_c_stack_check(mrb);\n"
      s << body << "}\n\n"
      s << "static mrb_value\n#{@cfunc}(mrb_state *mrb, mrb_value self)\n{\n"
      @params.each do |p|
        init = if p[:optional] then " = mrb_undef_value()"
               elsif p[:kind] == :value then " = mrb_nil_value()"
               else " = 0"
               end
        s << "  #{CTYPE[p[:kind]]} #{p[:c]}#{init};\n"
        s << "  mrb_value #{boxed_param(p)} = mrb_nil_value();\n" if p[:kind] != :value
      end
      @out = s
      emit_params
      args = param_names.map { |c| ", #{c}" }.join
      s << "  return #{impl_name}(mrb, self#{args});\n}\n"
      s
    end

    def self.sym_decls(syms)
      syms.values.map { |c| "static mrb_sym #{c};\n" }.join
    end

    # One slot per constant read site's (scope, path): it keeps the value
    # once that is a class or module (aot_cslot_fill).
    def self.const_decls(consts)
      consts.values.map { |c| "static struct aot_cslot #{c};\n" }.join
    end

    def const_slot(path)
      @consts[[@scope, path]] ||= "cc_#{@consts.size}"
    end

    def arity_spec
      pos = @params.reject { |p| p[:kw] || p[:block] }
      req = pos.count { |p| !p[:optional] }
      opt = pos.size - req
      spec = opt.zero? ? "MRB_ARGS_REQ(#{req})" : "MRB_ARGS_ARG(#{req}, #{opt})"
      nkw = @params.count { |p| p[:kw] }
      spec += " | MRB_ARGS_KEY(#{nkw}, 0)" unless nkw.zero?
      spec += " | MRB_ARGS_BLOCK()" if block?
      spec
    end

    private

    def unsupported(what, node = nil)
      loc = node ? " at #{@file}:#{node.location.start_line}" : ""
      raise Unsupported, "#{what}#{loc}"
    end

    def emit(line)
      @out << "  " << line << "\n"
    end

    def tmp(kind)
      @tmp += 1
      name = "t#{@tmp}"
      @locals["\0tmp#{@tmp}"] = [name, kind]
      name
    end

    # A right side with no effect: dropping its assignment loses nothing.
    PURE_RHS = /\A(mrb_(nil|true|false|undef)_value\(\)|\(\(aot_int\)-?\d+LL\)|-?\d+(\.\d+)?|[A-Za-z_]\w*)\z/

    # A local the body only assigns (the result slot of an `if` that is a
    # statement, a value the next statement never reads, a Ruby `_`) drew
    # -Wunused-but-set-variable on every such function. Such a local
    # loses its declaration; each assignment keeps its right side for the
    # effects, or goes when the right side has none. A parameter stays.
    # Returns the names dropped.
    def drop_unused_temps(body, param_names)
      dead = []
      temps = @locals.map { |_, (c, _)| c }.reject { |c| param_names.include?(c) }
      # To a fixpoint: `t12 = t11;` is t11's only read, and goes with t12.
      # (`loop` is this class's while-loop emitter, hence `while`.)
      changed = true
      while changed
        changed = false
        temps.each do |c|
          next if dead.include?(c)
          uses = body.scan(/\b#{c}\b/).size
          assigns = body.scan(/^\s*#{c} = .*;( \})?\n/).size
          next unless uses == assigns
          dead << c
          changed = true
          next if uses == 0
          body.gsub!(/^(\s*)#{c} = (.*);( \})?\n/) do
            indent, rhs, close = $1, $2, $3
            if rhs =~ PURE_RHS
              close ? "#{indent}#{close.strip}\n" : ""
            else
              "#{indent}(void)(#{rhs});#{close}\n"
            end
          end
        end
      end
      dead
    end

    def sym(name)
      @syms[name] ||= "sym_#{@syms.size}_#{name.to_s.gsub(/[^A-Za-z0-9_]/) { |c| "_%02x" % c.ord }}"
      "aot_sym(mrb, &#{@syms[name]}, #{c_string(name.to_s)})"
    end

    def c_string(s)
      '"' + s.bytes.map { |b|
        case b
        when 0x22 then '\\"'
        when 0x5c then '\\\\'
        when 0x0a then '\\n'
        when 0x0d then '\\r'
        when 0x09 then '\\t'
        when 0x20..0x7e then b.chr
        else "\\%03o" % b
        end
      }.join + '"'
    end

    # ---- kinds ---------------------------------------------------------

    # Spinel's opinion of a node, as a hint only.
    def hint(node)
      @types.kind(@file, node)
    end

    def box(v)
      case v.kind
      when :int then "AOT_BOX_INT(mrb, #{v.code})"
      when :float then "AOT_BOX_FLOAT(mrb, #{v.code})"
      when :bool then "AOT_BOX_BOOL(#{v.code})"
      when :nil then "mrb_nil_value()"
      else v.code
      end
    end

    # Convert to a slot kind. A boxed value never narrows silently: the
    # checked helpers raise TypeError when the word is not what the slot
    # holds, and the local-kind inference only gives a scalar slot to a
    # local every write of which is a scalar the C computed itself.
    def coerce(v, kind)
      return v.code if v.kind == kind
      case kind
      when :value then box(v)
      when :int
        v.kind == :float ? "aot_float_to_i(mrb, #{v.code})" : "aot_unbox_int(mrb, #{box(v)})"
      when :float
        v.kind == :int ? "((mrb_float)#{v.code})" : "aot_unbox_float(mrb, #{box(v)})"
      when :bool then truthy(v)
      when :nil then v.code
      end
    end

    def truthy(v)
      case v.kind
      when :bool then v.code
      when :int, :float then "1"
      when :nil then "0"
      else "mrb_test(#{v.code})"
      end
    end

    # A comparison is emitted as `(a == b)`; inside `if (...)` the second
    # pair draws clang's -Wparentheses-equality. A condition that is one
    # parenthesized expression goes in without its own pair.
    def strip_parens(code)
      return code unless code.start_with?("(") && code.end_with?(")")
      depth = 0
      code.each_char.with_index do |ch, i|
        depth += 1 if ch == "("
        depth -= 1 if ch == ")"
        return code if depth == 0 && i < code.size - 1
      end
      code[1..-2]
    end

    # ---- params --------------------------------------------------------

    def check_params
      p = @def.parameters
      @params = []
      return add_block_param(nil) unless p
      unsupported("rest parameter") if p.rest
      unsupported("post parameters") unless p.posts.empty?
      unsupported("keyword rest parameter") if p.keyword_rest
      p.requireds.each do |r|
        unsupported("destructuring parameter") unless r.is_a?(Prism::RequiredParameterNode)
        c = "p_#{r.name}"
        kind = @types.scalar(@file, r)
        kind = :value if kind.nil? || param_tested?(r.name)
        @params << { name: r.name.to_s, c: c, kind: kind }
        @locals[r.name.to_s] = [c, kind]
      end
      # An optional parameter stays boxed; the cfunc hands undef to the impl
      # for an argument not given, and the impl evaluates the default (in
      # method scope, as Ruby does: it may read the earlier parameters).
      p.optionals.each do |o|
        c = "p_#{o.name}"
        @params << { name: o.name.to_s, c: c, kind: :value, optional: true, default: o.value }
        @locals[o.name.to_s] = [c, :value]
      end
      # Keyword parameters follow the positionals in the impl's argument
      # list, in declaration order. mrb_get_args ":" reads them: a required
      # keyword is always present (it raises otherwise) and takes its proved
      # kind; an optional one is undef when not given and keeps its boxed
      # slot, with the default evaluated as for an optional positional.
      p.keywords.each do |k|
        c = "p_#{k.name}"
        if k.is_a?(Prism::RequiredKeywordParameterNode)
          kind = @types.scalar(@file, k)
          kind = :value if kind.nil? || param_tested?(k.name)
          @params << { name: k.name.to_s, c: c, kind: kind, kw: true }
        else
          kind = :value
          @params << { name: k.name.to_s, c: c, kind: :value, kw: true, optional: true, default: k.value }
        end
        @locals[k.name.to_s] = [c, kind]
      end
      add_block_param(p.block)
    end

    # The block the cfunc receives, last in the impl's argument list: the
    # `&blk` parameter (a Proc or nil, a local of the body), or a hidden
    # one when the body has `yield` or `block_given?` and no `&blk`. The
    # block is read with mrb_get_args "&" and is never unboxed; whatever
    # the caller's block returns stays a boxed value (see yield_call).
    def add_block_param(block_node)
      if block_node
        unsupported("anonymous block parameter") if block_node.name.nil?
        @params << { name: block_node.name.to_s, c: BLOCK_C, kind: :value, block: true }
        @locals[block_node.name.to_s] = [BLOCK_C, :value]
      elsif uses_block?(@def.body)
        @params << { name: nil, c: BLOCK_C, kind: :value, block: true }
      end
    end

    def uses_block?(node)
      return false if node.nil?
      return true if node.is_a?(Prism::YieldNode)
      return true if node.is_a?(Prism::CallNode) && node.receiver.nil? && node.name == :block_given? && node.arguments.nil?
      node.compact_child_nodes.any? { |c| uses_block?(c) }
    end


    # Every argument is read boxed ("o"); a proved parameter is then
    # unboxed by the checked helper, which takes the proved class and
    # nothing else (no Float truncation for an Integer, no truthiness for a
    # bool, no Integer for a Float). A caller the analysis never saw, or an
    # RBS that misstates a class, thus raises TypeError here instead of
    # running the body on a reinterpreted value.
    PARAM_UNBOX = { int: "aot_unbox_int", float: "aot_unbox_float", bool: "aot_unbox_bool" }.freeze

    def boxed_param(p)
      "v_#{p[:name]}"
    end

    def emit_params
      return if @params.empty?
      pos = @params.reject { |p| p[:kw] || p[:block] }
      kws = @params.select { |p| p[:kw] }
      req = pos.count { |p| !p[:optional] }
      fmt = "o" * req
      fmt += "|" + "o" * (pos.size - req) if req < pos.size
      refs = pos.map { |p| ", &#{p[:kind] == :value ? p[:c] : boxed_param(p)}" }.join
      if block?
        fmt += "&"
        refs += ", &#{BLOCK_C}"
      end
      unless kws.empty?
        # Required keywords first in the table, as mrb_kwargs wants them.
        kws = kws.sort_by { |p| p[:optional] ? 1 : 0 }
        names = kws.map { |p| sym(p[:name]) }.join(", ")
        emit "mrb_sym kw_names[#{kws.size}] = { #{names} };"
        emit "mrb_value kw_vals[#{kws.size}];"
        emit "mrb_kwargs kw = { #{kws.size}, #{kws.count { |p| !p[:optional] }}, kw_names, kw_vals, NULL };"
        fmt += ":"
        refs += ", &kw"
      end
      emit "mrb_get_args(mrb, #{fmt.inspect}#{refs});"
      kws.each_with_index do |p, i|
        emit "#{p[:kind] == :value ? p[:c] : boxed_param(p)} = kw_vals[#{i}];"
      end
      @params.each do |p|
        next if p[:kind] == :value
        emit "#{p[:c]} = #{PARAM_UNBOX[p[:kind]]}(mrb, #{boxed_param(p)});"
      end
    end

    def emit_defaults
      @params.each do |p|
        next unless p[:optional]
        emit "if (mrb_undef_p(#{p[:c]})) {"
        v = expr(p[:default])
        emit "#{p[:c]} = #{box(v)};"
        emit "}"
      end
    end

    # Does the body ask the parameter what it is? A guard such as
    # `value.is_a?(Integer)` or `case value when Integer` is written for a
    # caller the analysis never saw, and the irep answers it with the
    # method's own error; an unboxed read would answer with TypeError
    # before the body runs. Such a parameter stays boxed.
    CLASS_TESTS = %w[is_a? kind_of? instance_of? nil? respond_to? class].freeze

    def param_tested?(name)
      tested = false
      walk = lambda do |node|
        return if tested || node.nil?
        case node
        when Prism::CallNode
          recv = node.receiver
          if recv.is_a?(Prism::LocalVariableReadNode) && recv.name == name &&
             (CLASS_TESTS.include?(node.name.to_s) || node.safe_navigation?)
            tested = true
          elsif node.name == :=== && node.arguments && node.arguments.arguments.size == 1 &&
                node.arguments.arguments[0].is_a?(Prism::LocalVariableReadNode) && node.arguments.arguments[0].name == name
            tested = true
          end
        when Prism::CaseNode
          tested = true if node.predicate.is_a?(Prism::LocalVariableReadNode) && node.predicate.name == name
        end
        node.child_nodes.each { |c| walk.call(c) }
      end
      walk.call(@def.body)
      tested
    end

    # ---- local kinds ---------------------------------------------------

    # The kind a local gets: the kind of the values written to it when the C
    # holds every one of them unboxed (literals, arithmetic on such values,
    # other such locals, unboxed parameters, and reads Spinel proved
    # non-nil scalars), else mrb_value. A fixpoint over the writes, since a
    # local can feed another.
    def infer_local_kinds(body)
      writes = []
      collect_writes(body, writes)
      kinds = {}
      writes.each { |name, _| kinds[name] = nil unless @locals.key?(name) }
      # A cell local holds a boxed value whatever it is assigned.
      @cells.each_key { |name| kinds[name] = :value if kinds.key?(name) }
      3.times do
        changed = false
        writes.each do |name, value|
          next if @locals.key?(name) # a parameter: its kind is fixed at entry
          k = static_kind(value, kinds)
          k = :value if k == :nil
          cur = kinds[name]
          new = cur.nil? ? k : (cur == k ? cur : :value)
          if new != cur
            kinds[name] = new
            changed = true
          end
        end
        break unless changed
      end
      kinds.each { |name, k| @locals[name] = ["l_#{name}", k || :value] }
    end

    def collect_writes(node, acc)
      return unless node
      case node
      when Prism::LocalVariableWriteNode
        acc << [node.name.to_s, node.value]
      when Prism::LocalVariableOperatorWriteNode
        acc << [node.name.to_s, node]
      when Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode
        acc << [node.name.to_s, nil]
      when Prism::MultiWriteNode
        node.lefts.each { |t| acc << [t.name.to_s, nil] if t.is_a?(Prism::LocalVariableTargetNode) }
      when Prism::BlockNode
        return # compiled apart, with locals of its own (block_literal)
      when Prism::LambdaNode
        unsupported("lambda", node)
      end
      node.child_nodes.each { |c| collect_writes(c, acc) }
    end

    def static_kind(node, kinds)
      case node
      when nil then :value
      when Prism::IntegerNode then :int
      when Prism::FloatNode then :float
      when Prism::TrueNode, Prism::FalseNode then :bool
      when Prism::NilNode then :nil
      when Prism::ParenthesesNode
        node.body && node.body.body.size == 1 ? static_kind(node.body.body.first, kinds) : :value
      when Prism::LocalVariableReadNode
        name = node.name.to_s
        if @locals.key?(name) then @locals[name][1]
        elsif kinds.key?(name) then kinds[name] || :value
        else :value
        end
      when Prism::LocalVariableOperatorWriteNode
        name = node.name.to_s
        l = if @locals.key?(name) then @locals[name][1]
            elsif kinds.key?(name) then kinds[name] || :value
            else :value
            end
        numeric_result(node.binary_operator.to_s, l, static_kind(node.value, kinds))
      when Prism::InstanceVariableReadNode
        proved_kind(node) || :value
      when Prism::CallNode
        return :value if node.block || node.safe_navigation?
        if node.receiver.nil?
          return :value if node.name == :raise
          return proved_kind(node) || :value
        end
        args = node.arguments ? node.arguments.arguments : []
        l = static_kind(node.receiver, kinds)
        k = if args.size == 1
              numeric_result(node.name.to_s, l, static_kind(args[0], kinds))
            elsif args.empty?
              case [l, node.name.to_s]
              when [:int, "-@"], [:int, "to_i"], [:int, "abs"], [:float, "to_i"] then :int
              when [:float, "-@"], [:float, "to_f"], [:int, "to_f"] then :float
              when [:int, "zero?"], [:int, "even?"], [:int, "odd?"], [:bool, "!"] then :bool
              else :value
              end
            else
              :value
            end
        k == :value ? (proved_kind(node) || :value) : k
      else
        :value
      end
    end

    # :int or :float when Spinel proved the node's value a non-nil scalar
    # (the kinds a value from the VM is unboxed to); nil otherwise. A bool
    # from the VM stays boxed: `mrb_test` reads it where it is tested, and
    # boxing it back would turn a truthy non-Boolean into true.
    def proved_kind(node)
      k = @types.scalar(@file, node)
      k == :int || k == :float ? k : nil
    end

    # Mark a value the VM produced with the kind Spinel proved for it, so a
    # later use that needs the scalar can unbox it; the value stays boxed
    # here.
    def unbox_proved(node, v)
      return v unless v.kind == :value && v.proved.nil?
      k = proved_kind(node)
      k ? Val.new(v.code, :value, k) : v
    end

    # The unboxed form of a value: itself when already unboxed, a checked
    # unbox when proved, nil when it has to stay boxed.
    def materialize(v)
      return v if v.kind != :value
      return nil unless v.proved
      t = tmp(v.proved)
      emit "#{t} = aot_unbox_#{v.proved}(mrb, #{v.code});"
      Val.new(t, v.proved)
    end

    # The kind a value has once materialized, or :value.
    def use_kind(v)
      v.kind == :value ? (v.proved || :value) : v.kind
    end

    def numeric_result(op, l, r)
      return :value if l == :value || r == :value || l == :nil || r == :nil || l == :bool || r == :bool
      if l == :int && r == :int
        return :int if INT_BINOPS[op] || INT_BITOPS[op]
        return :bool if CMP_OPS[op]
        return :value
      end
      return :float if FLOAT_BINOPS[op] || op == "%" || op == "**"
      return :bool if CMP_OPS[op]
      :value
    end

    def local(name)
      @locals[name.to_s] ||= ["l_#{name}", :value]
    end

    # ---- statements ----------------------------------------------------

    def statements(node)
      return Val.new("mrb_nil_value()", :nil) if node.nil?
      stmts = node.is_a?(Prism::StatementsNode) ? node.body : [node]
      last = Val.new("mrb_nil_value()", :nil)
      stmts.each_with_index do |s, i|
        if i == stmts.size - 1
          last = expr(s)
        else
          v = expr(s)
          emit "(void)(#{v.code});" unless v.kind == :nil
        end
      end
      last
    end

    # Compile a subtree into its own buffer; returns [code, Val].
    def capture
      saved = @out
      @out = +""
      v = yield
      code = @out
      @out = saved
      [code, v]
    end

    # ---- expressions ---------------------------------------------------

    def expr(node)
      case node
      when Prism::IntegerNode
        unsupported("integer literal out of 64-bit range", node) if node.value.abs > INT_LIT_MAX
        Val.new("((aot_int)#{node.value}LL)", :int)
      when Prism::FloatNode
        Val.new("((mrb_float)#{format('%.17g', node.value)})", :float)
      when Prism::NilNode then Val.new("mrb_nil_value()", :nil)
      when Prism::TrueNode then Val.new("1", :bool)
      when Prism::FalseNode then Val.new("0", :bool)
      when Prism::SelfNode then Val.new("self", :value)
      when Prism::StringNode
        unsupported("non-ASCII/binary string literal", node) unless node.unescaped.valid_encoding?
        t = tmp(:value)
        emit "#{t} = mrb_str_new_lit(mrb, #{c_string(node.unescaped)});"
        Val.new(t, :value)
      when Prism::InterpolatedStringNode
        # "a#{x}b": a new String, each literal part appended as bytes and
        # each embedded expression through mrb_str_concat, which converts
        # with to_s as the VM's OP_STRCAT does.
        t = tmp(:value)
        emit "#{t} = mrb_str_new(mrb, NULL, 0);"
        node.parts.each do |part|
          case part
          when Prism::StringNode
            unsupported("non-ASCII/binary string literal", part) unless part.unescaped.valid_encoding?
            emit "mrb_str_cat_lit(mrb, #{t}, #{c_string(part.unescaped)});" unless part.unescaped.empty?
          when Prism::EmbeddedStatementsNode
            v = part.statements ? statements(part.statements) : Val.new("mrb_nil_value()", :nil)
            emit "mrb_str_concat(mrb, #{t}, #{box(v)});"
          else
            unsupported("#{part.class.name.split('::').last} in a string", part)
          end
        end
        Val.new(t, :value)
      when Prism::SymbolNode
        Val.new("mrb_symbol_value(#{sym(node.unescaped)})", :value)
      when Prism::ArrayNode
        unsupported("array with splat", node) if node.elements.any? { |e| e.is_a?(Prism::SplatNode) }
        vals = node.elements.map { |e| box(expr(e)) }
        t = tmp(:value)
        if vals.empty?
          emit "#{t} = mrb_ary_new(mrb);"
        else
          a = "av#{@tmp}"
          emit "{ mrb_value #{a}[] = { #{vals.join(', ')} }; #{t} = mrb_ary_new_from_values(mrb, #{vals.size}, #{a}); }"
        end
        Val.new(t, :value)
      when Prism::HashNode
        # As OP_HASH: a new Hash, each pair set in order with mrb_hash_set
        # (which dups and freezes a String key as the VM does).
        t = tmp(:value)
        emit "#{t} = mrb_hash_new_capa(mrb, #{node.elements.size});"
        node.elements.each do |e|
          unsupported("double splat in a hash literal", e) unless e.is_a?(Prism::AssocNode)
          k = box(expr(e.key))
          v = box(expr(e.value))
          emit "mrb_hash_set(mrb, #{t}, #{k}, #{v});"
        end
        Val.new(t, :value)
      when Prism::GlobalVariableReadNode
        t = tmp(:value)
        emit "#{t} = mrb_gv_get(mrb, #{sym(node.name)});"
        Val.new(t, :value)
      when Prism::GlobalVariableWriteNode
        t = tmp(:value)
        emit "#{t} = #{box(expr(node.value))};"
        emit "mrb_gv_set(mrb, #{sym(node.name)}, #{t});"
        Val.new(t, :value)
      when Prism::ParenthesesNode
        node.body ? statements(node.body) : Val.new("mrb_nil_value()", :nil)
      when Prism::StatementsNode
        statements(node)
      when Prism::LocalVariableReadNode
        return cell_read(node) if cell?(node.name)
        c, k = local(node.name)
        Val.new(c, k)
      when Prism::LocalVariableWriteNode
        return cell_write(node, expr(node.value)) if cell?(node.name)
        c, k = local(node.name)
        v = expr(node.value)
        check_plain_write(node)
        emit "#{c} = #{coerce(v, k)};"
        Val.new(c, k)
      when Prism::LocalVariableOperatorWriteNode
        if cell?(node.name)
          cur = cell_read(node)
          return cell_write(node, binop(node.binary_operator.to_s, cur, expr(node.value), node))
        end
        c, k = local(node.name)
        v = binop(node.binary_operator.to_s, Val.new(c, k), expr(node.value), node)
        check_plain_write(node)
        emit "#{c} = #{coerce(v, k)};"
        Val.new(c, k)
      when Prism::LocalVariableOrWriteNode
        if cell?(node.name)
          c, = local(node.name)
          t = tmp(:value)
          emit "#{t} = aot_cell_get(mrb, #{c});"
          emit "if (!mrb_test(#{t})) {"
          v = expr(node.value)
          emit "#{t} = #{box(v)};"
          emit "aot_cell_set(mrb, #{c}, #{t});"
          emit "}"
          return Val.new(t, :value)
        end
        c, k = local(node.name)
        unsupported("||= on an unboxed local", node) unless k == :value
        check_plain_write(node)
        emit "if (!mrb_test(#{c})) {"
        v = expr(node.value)
        emit "#{c} = #{box(v)};"
        emit "}"
        Val.new(c, :value)
      when Prism::InstanceVariableReadNode
        t = tmp(:value)
        emit "#{t} = aot_iv_get(mrb, self, #{sym(node.name)});"
        unbox_proved(node, Val.new(t, :value))
      when Prism::InstanceVariableWriteNode
        v = expr(node.value)
        t = tmp(:value)
        emit "#{t} = #{box(v)};"
        emit "mrb_iv_set(mrb, self, #{sym(node.name)}, #{t});"
        Val.new(t, :value)
      when Prism::InstanceVariableOperatorWriteNode
        cur = tmp(:value)
        emit "#{cur} = aot_iv_get(mrb, self, #{sym(node.name)});"
        v = binop(node.binary_operator.to_s, Val.new(cur, :value), expr(node.value), node)
        t = tmp(:value)
        emit "#{t} = #{box(v)};"
        emit "mrb_iv_set(mrb, self, #{sym(node.name)}, #{t});"
        Val.new(t, :value)
      when Prism::InstanceVariableOrWriteNode
        t = tmp(:value)
        emit "#{t} = aot_iv_get(mrb, self, #{sym(node.name)});"
        emit "if (!mrb_test(#{t})) {"
        v = expr(node.value)
        emit "#{t} = #{box(v)};"
        emit "mrb_iv_set(mrb, self, #{sym(node.name)}, #{t});"
        emit "}"
        Val.new(t, :value)
      when Prism::ConstantReadNode
        # The slot keeps the value once it is a class or module: the
        # lookup (each lexical scope, then the ancestors) runs once per
        # site, as a class is not reassigned. Any other value (a Hash, a
        # String, an Integer) is looked up every time, as the VM does.
        t = tmp(:value)
        slot = const_slot(node.name.to_s)
        scope = @scope.empty? ? "NULL" : "&#{Compiler.path_name(@paths, @scope)}"
        emit "if (#{slot}.set) { #{t} = #{slot}.v; } else {"
        emit "  #{t} = aot_const_get(mrb, self, #{scope}, #{sym(node.name)});"
        emit "  aot_cslot_fill(&#{slot}, #{t});"
        emit "}"
        Val.new(t, :value)
      when Prism::ConstantPathNode
        # As the VM reads `A::B::C`: the first name lexically (OP_GETCONST),
        # each later one off the value before it (OP_GETMCNST, which raises
        # TypeError when that value is not a class or module). The last
        # need not be a class: `IRQ::HANDLER` is a Hash. The whole path
        # has one slot, as a ConstantReadNode has.
        parts = constant_path(node)
        t = tmp(:value)
        slot = const_slot(parts.map { |p| p.nil? ? "" : p }.join("::"))
        emit "if (#{slot}.set) { #{t} = #{slot}.v; } else {"
        if parts.first.nil?
          emit "  #{t} = mrb_obj_value(mrb->object_class);"
          parts.shift
        else
          scope = @scope.empty? ? "NULL" : "&#{Compiler.path_name(@paths, @scope)}"
          emit "  #{t} = aot_const_get(mrb, self, #{scope}, #{sym(parts.shift)});"
        end
        parts.each { |name| emit "  #{t} = mrb_const_get(mrb, #{t}, #{sym(name)});" }
        emit "  aot_cslot_fill(&#{slot}, #{t});"
        emit "}"
        Val.new(t, :value)
      when Prism::IfNode then conditional(node, false)
      when Prism::UnlessNode then conditional(node, true)
      when Prism::CaseNode then case_when(node)
      when Prism::MultiWriteNode then multi_write(node)
      when Prism::RangeNode
        unsupported("endless or beginless range", node) if node.left.nil? || node.right.nil?
        lo = expr(node.left)
        hi = expr(node.right)
        t = tmp(:value)
        emit "#{t} = mrb_range_new(mrb, #{box(lo)}, #{box(hi)}, #{node.exclude_end? ? 1 : 0});"
        Val.new(t, :value)
      when Prism::WhileNode then loop(node, false)
      when Prism::UntilNode then loop(node, true)
      when Prism::BeginNode then begin_block(node)
      when Prism::ReturnNode
        unsupported("return in a block", node) if @in_block
        unsupported("return with several values", node) if node.arguments && node.arguments.arguments.size > 1
        v = node.arguments ? expr(node.arguments.arguments.first) : Val.new("mrb_nil_value()", :nil)
        emit_return(box(v))
        Val.new("mrb_nil_value()", :nil)
      when Prism::BreakNode
        unsupported("break with a value", node) if node.arguments
        emit_break(node)
        Val.new("mrb_nil_value()", :nil)
      when Prism::NextNode
        if @in_block && @loop_depth.zero?
          # `next v` ends this call of the block with v.
          unsupported("next with several values", node) if node.arguments && node.arguments.arguments.size > 1
          v = node.arguments ? expr(node.arguments.arguments.first) : Val.new("mrb_nil_value()", :nil)
          emit "return #{box(v)};"
          return Val.new("mrb_nil_value()", :nil)
        end
        if @inline && @loop_depth == @inline[:depth]
          # `next v` in an inline loop's body ends this turn with v.
          unsupported("next with several values", node) if node.arguments && node.arguments.arguments.size > 1
          v = node.arguments ? expr(node.arguments.arguments.first) : Val.new("mrb_nil_value()", :nil)
          emit "#{@inline[:bv]} = #{box(v)};"
          emit "goto #{@inline[:label]};"
          @inline[:used] = true
          return Val.new("mrb_nil_value()", :nil)
        end
        emit_next(node)
        Val.new("mrb_nil_value()", :nil)
      when Prism::AndNode then logical(node, "&&")
      when Prism::OrNode then logical(node, "||")
      when Prism::CallNode then call(node)
      when Prism::YieldNode then yield_call(node)
      else
        unsupported("#{node.class.name.split('::').last}", node)
      end
    end

    # The names of `A::B::C`, outermost first; a leading nil for an
    # absolute path (`::A::B`).
    def constant_path(node)
      parts = []
      n = node
      while n.is_a?(Prism::ConstantPathNode)
        parts.unshift(n.name.to_s)
        n = n.parent
      end
      case n
      when nil then parts.unshift(nil)
      when Prism::ConstantReadNode then parts.unshift(n.name.to_s)
      else unsupported("dynamic constant path", node)
      end
      parts
    end

    # The kind two branches agree on, else mrb_value (nil joined with a
    # scalar has no unboxed form).
    def join_kind(a, b)
      a == b ? a : :value
    end

    def conditional(node, negate)
      c = expr(node.predicate)
      cond = truthy(c)
      cond = "!(#{cond})" if negate
      then_code, v1 = capture { node.statements ? statements(node.statements) : Val.new("mrb_nil_value()", :nil) }
      sub = node.respond_to?(:subsequent) ? node.subsequent : node.else_clause
      else_code, v2 = capture do
        case sub
        when nil then Val.new("mrb_nil_value()", :nil)
        when Prism::ElseNode then sub.statements ? statements(sub.statements) : Val.new("mrb_nil_value()", :nil)
        when Prism::IfNode then conditional(sub, false)
        else unsupported("#{sub.class}", sub)
        end
      end
      kind = join_kind(v1.kind, v2.kind)
      result = kind == :nil ? nil : tmp(kind)
      emit "if (#{strip_parens(cond)}) {"
      @out << then_code
      emit "#{result} = #{coerce(v1, kind)};" if result
      emit "} else {"
      @out << else_code
      emit "#{result} = #{coerce(v2, kind)};" if result
      emit "}"
      result ? Val.new(result, kind) : Val.new("mrb_nil_value()", :nil)
    end

    # case/when as an if/else-if chain. Each arm tests `pattern === value`
    # through aot_eqq; an Integer pattern against an unboxed Integer value
    # is a plain C compare. The arms' values join as an if's branches do.
    def case_when(node)
      unsupported("case without a subject", node) if node.predicate.nil?
      subj = expr(node.predicate)
      sk = subj.kind == :value ? (subj.proved || :value) : subj.kind
      sv = tmp(sk == :value ? :value : sk)
      emit "#{sv} = #{coerce(subj, sk)};"
      subject = Val.new(sv, sk)
      arms = node.conditions.map do |w|
        unsupported("when with a splat", w) if w.conditions.any? { |c| c.is_a?(Prism::SplatNode) }
        test_code, test = capture { case_test(w.conditions, subject) }
        body_code, v = capture { w.statements ? statements(w.statements) : Val.new("mrb_nil_value()", :nil) }
        [test_code, test, body_code, v]
      end
      else_code, ev = capture do
        node.else_clause && node.else_clause.statements ? statements(node.else_clause.statements) : Val.new("mrb_nil_value()", :nil)
      end
      kind = (arms.map { |a| a[3].kind } + [ev.kind]).uniq
      kind = kind.size == 1 ? kind.first : :value
      result = kind == :nil ? nil : tmp(kind)
      # Nested, not chained: an arm's pattern evaluations must run only
      # when the arms before it did not match.
      arms.each_with_index do |(test_code, test, body_code, v), i|
        emit "} else {" if i > 0
        @out << test_code
        emit "if (#{strip_parens(test)}) {"
        @out << body_code
        emit "#{result} = #{coerce(v, kind)};" if result
      end
      emit "} else {" unless arms.empty?
      @out << else_code
      emit "#{result} = #{coerce(ev, kind)};" if result
      arms.size.times { emit "}" }
      result ? Val.new(result, kind) : Val.new("mrb_nil_value()", :nil)
    end

    # The C condition for one arm: its patterns or-ed. Emits the pattern
    # evaluations; the test expression itself is pure.
    def case_test(conditions, subject)
      conditions.map do |c|
        lo, hi, excl = int_range_literal(c)
        if subject.kind == :int && c.is_a?(Prism::IntegerNode)
          "(#{subject.code} == ((aot_int)#{c.value}LL))"
        elsif subject.kind == :int && lo
          "(((aot_int)#{lo}LL) <= #{subject.code} && #{subject.code} #{excl ? '<' : '<='} ((aot_int)#{hi}LL))"
        elsif subject.kind == :value && c.is_a?(Prism::IntegerNode)
          "aot_eqq_int(mrb, #{subject.code}, ((aot_int)#{c.value}LL))"
        elsif subject.kind == :value && lo
          "aot_eqq_int_range(mrb, #{subject.code}, ((aot_int)#{lo}LL), ((aot_int)#{hi}LL), #{excl ? 1 : 0})"
        else
          pat = expr(c)
          t = tmp(:value)
          emit "#{t} = aot_eqq(mrb, #{box(pat)}, #{box(subject)});"
          "mrb_test(#{t})"
        end
      end.join(" || ")
    end

    # [lo, hi, exclusive] for a `1..9` / `1...9` of Integer literals, else nil.
    def int_range_literal(node)
      return nil unless node.is_a?(Prism::RangeNode)
      return nil unless node.left.is_a?(Prism::IntegerNode) && node.right.is_a?(Prism::IntegerNode)
      return nil if node.left.value.abs > INT_LIT_MAX || node.right.value.abs > INT_LIT_MAX
      [node.left.value, node.right.value, node.exclude_end?]
    end

    # `a, b = x, y` and `a, b = expr`: the right side first (into temps, so
    # `a, b = b, a` swaps), then each target. Locals and instance variables
    # only; the value of the statement is nil here (it is rarely used).
    def multi_write(node)
      unsupported("splat in a multiple assignment", node) if node.rest || !node.rights.empty?
      targets = node.lefts
      targets.each do |t|
        next if t.is_a?(Prism::LocalVariableTargetNode) || t.is_a?(Prism::InstanceVariableTargetNode)
        unsupported("#{t.class.name.split('::').last} in a multiple assignment", t)
      end
      values =
        if node.value.is_a?(Prism::ArrayNode) && node.value.elements.none? { |e| e.is_a?(Prism::SplatNode) }
          node.value.elements.map do |e|
            v = expr(e)
            t = tmp(:value)
            emit "#{t} = #{box(v)};"
            Val.new(t, :value)
          end
        else
          v = expr(node.value)
          src = tmp(:value)
          emit "#{src} = #{box(v)};"
          targets.each_index.map do |i|
            t = tmp(:value)
            emit "#{t} = aot_masgn_get(mrb, #{src}, #{i});"
            Val.new(t, :value)
          end
        end
      targets.each_with_index do |t, i|
        write_target(t, values[i] || Val.new("mrb_nil_value()", :nil))
      end
      Val.new("mrb_nil_value()", :nil)
    end

    # Assign `v` to a target node: a local (its cell when it has one) or
    # an instance variable.
    def write_target(t, v)
      case t
      when Prism::LocalVariableTargetNode
        return cell_write(t, v) if cell?(t.name)
        c, k = local(t.name)
        check_plain_write(t)
        emit "#{c} = #{coerce(v, k)};"
      when Prism::InstanceVariableTargetNode
        emit "mrb_iv_set(mrb, self, #{sym(t.name)}, #{box(v)});"
      else
        unsupported("#{t.class.name.split('::').last} as an assignment target", t)
      end
    end

    # `return v` leaves the method; inside a begin's C function it is
    # recorded as a jump and the function returns v as its result.
    def emit_return(code)
      if @ud
        @jumps[:return] = true
        emit "#{@ud}->jump = AOT_JUMP_RETURN;"
      end
      emit "return #{code};"
    end

    # `break` and `next` go to the C loop when one is open in this
    # function; inside a begin's C function with the loop outside they
    # are recorded as a jump the method's C makes after the begin.
    def emit_break(node)
      if @loop_depth > 0
        emit "break;"
      elsif @in_block
        unsupported("break in a block", node)
      elsif @ud && @outer_loops > 0
        @jumps[:break] = true
        emit "#{@ud}->jump = AOT_JUMP_BREAK;"
        emit "return mrb_nil_value();"
      else
        unsupported("break outside a while loop", node)
      end
    end

    def emit_next(node)
      if @loop_depth > 0
        emit "continue;"
      elsif @ud && @outer_loops > 0
        @jumps[:next] = true
        emit "#{@ud}->jump = AOT_JUMP_NEXT;"
        emit "return mrb_nil_value();"
      else
        unsupported("next outside a while loop", node)
      end
    end

    def loop(node, negate)
      unsupported("do-while loop", node) if node.respond_to?(:begin_modifier?) && node.begin_modifier?
      emit "for (;;) {"
      c = expr(node.predicate)
      cond = truthy(c)
      cond = negate ? strip_parens(cond) : "!(#{cond})"
      emit "if (#{cond}) break;"
      @loop_depth += 1
      @loops << node
      statements(node.statements) if node.statements
      @loops.pop
      @loop_depth -= 1
      emit "}"
      Val.new("mrb_nil_value()", :nil)
    end

    def logical(node, op)
      l = expr(node.left)
      right_code, r = capture { expr(node.right) }
      if l.kind == :bool && r.kind == :bool
        t = tmp(:bool)
        emit "#{t} = #{l.code};"
        emit(op == "&&" ? "if (#{t}) {" : "if (!#{t}) {")
        @out << right_code
        emit "#{t} = #{r.code};"
        emit "}"
        Val.new(t, :bool)
      else
        t = tmp(:value)
        emit "#{t} = #{box(l)};"
        emit(op == "&&" ? "if (mrb_test(#{t})) {" : "if (!mrb_test(#{t})) {")
        @out << right_code
        emit "#{t} = #{box(r)};"
        emit "}"
        Val.new(t, :value)
      end
    end

    # ---- calls ---------------------------------------------------------

    # `recv&.m(args)`: the receiver once, then nil without a look at the
    # arguments, or the call. The result stays boxed (nil joins it).
    def safe_call(node)
      recv = expr(node.receiver)
      return call(node, recv) if recv.kind != :value && recv.kind != :nil
      t = tmp(:value)
      emit "if (mrb_nil_p(#{box(recv)})) {"
      emit "  #{t} = mrb_nil_value();"
      emit "} else {"
      v = call(node, recv)
      emit "  #{t} = #{box(v)};"
      emit "}"
      Val.new(t, :value)
    end

    def call(node, recv = nil)
      return safe_call(node) if node.safe_navigation? && recv.nil?
      args = node.arguments ? node.arguments.arguments : []
      kw_node = nil
      args.each do |a|
        unsupported("splat argument", node) if a.is_a?(Prism::SplatNode)
        kw_node = a if a.is_a?(Prism::KeywordHashNode)
      end
      args -= [kw_node] if kw_node
      name = node.name.to_s

      return raise_call(node, args) if node.receiver.nil? && name == "raise" && kw_node.nil? && node.block.nil?
      if node.receiver.nil? && name == "block_given?" && args.empty? && kw_node.nil? && node.block.nil?
        return Val.new("(!mrb_nil_p(#{BLOCK_C}))", :bool)
      end

      recv ||= node.receiver ? expr(node.receiver) : Val.new("self", :value)
      argv = args.map { |a| expr(a) }
      # `k: v` pairs, evaluated after the positionals as Ruby does. Only
      # symbol keys: a `**h` or a computed key takes the irep.
      kw = []
      if kw_node
        kw_node.elements.each do |e|
          unsupported("double splat argument", node) unless e.is_a?(Prism::AssocNode) && e.key.is_a?(Prism::SymbolNode)
          kw << [e.key.unescaped, expr(e.value)]
        end
      end
      # An iteration whose block can run in place: the loop and the body
      # in the method's own C (inline_loop), the Proc path behind a
      # run-time test of the receiver.
      if node.block.is_a?(Prism::BlockNode) && node.receiver && argv.empty? && kw.empty? &&
         LOOP_METHODS[name] && !@in_block && inline_loop_ok?(node.block) && direct_target(node, argv, kw).nil?
        return inline_loop(node, recv, name)
      end
      # `&blk` or the block literal last, as Ruby evaluates it.
      blk = node.block.is_a?(Prism::BlockNode) ? block_literal(node, node.block) : block_pass(node)
      return call_with_block(node, recv, name, argv, kw, blk) if blk

      if node.receiver && argv.size == 1 && kw.empty?
        v = binop(name, recv, argv[0], node, hint: true)
        return v if v
      end
      if node.receiver && argv.empty? && kw.empty?
        if recv.kind == :value && recv.proved && numeric_unop(name, Val.new("", recv.proved))
          return numeric_unop(name, materialize(recv))
        end
        v = numeric_unop(name, recv)
        return unbox_proved(node, v) if v
      end

      if node.receiver && kw.empty?
        raw = @types.raw_type(@file, node.receiver)
        helper = if raw == "string" then STR_METHODS[[name, argv.size]]
                 elsif raw.to_s.end_with?("_array") then ARY_METHODS[[name, argv.size]]
                 elsif raw.to_s.end_with?("_hash") then HASH_METHODS[[name, argv.size]]
                 end
        if helper
          t = tmp(:value)
          emit "#{t} = #{helper}(mrb, #{([box(recv)] + argv.map { |x| box(x) }).join(', ')});"
          return argv.last if name == "[]="
          return unbox_proved(node, Val.new(t, :value))
        end
        # the receiver untyped: the helper decides by its class at run time
        helper = GENERIC_METHODS[[name, argv.size]]
        if helper && direct_target(node, argv, kw).nil?
          t = tmp(:value)
          emit "#{t} = #{helper}(mrb, #{([box(recv)] + argv.map { |x| box(x) }).join(', ')});"
          return argv.last if name == "[]="
          return unbox_proved(node, Val.new(t, :value))
        end
      end

      t = tmp(:value)
      dispatch(node, t, recv, name, argv, kw, nil)
      # `recv.attr = v` and `recv[i] = v` evaluate to v.
      if node.receiver && name.end_with?("=") && !%w[== != <= >= ===].include?(name)
        return argv.last
      end
      # The result of a call on a Proc (`blk.call(x)`) is the caller's
      # block's to decide: it stays boxed whatever Spinel saw at the call
      # sites it analyzed.
      return Val.new(t, :value) if node.receiver && @types.raw_type(@file, node.receiver) == "proc"
      unbox_proved(node, Val.new(t, :value))
    end

    # `&expr` at a call site: the block the call forwards, as a C temp that
    # holds a Proc or nil (aot_block_arg converts a Symbol or any other
    # object with to_proc, as the VM does at the send). nil when the call
    # has no `&`.
    def block_pass(node)
      return nil unless node.block
      unsupported("anonymous block forwarding", node) if node.block.expression.nil?
      v = expr(node.block.expression)
      t = tmp(:value)
      emit "#{t} = aot_block_arg(mrb, #{box(v)});"
      Val.new(t, :value)
    end

    # `recv.m(args, &blk)`: the operator and core shortcuts take no block,
    # so the call goes to a compiled def's impl (the block as its last
    # argument) or to the VM with mrb_funcall_with_block.
    def call_with_block(node, recv, name, argv, kw, blk)
      t = tmp(:value)
      dispatch(node, t, recv, name, argv, kw, blk)
      Val.new(t, :value)
    end

    # ---- inline loops --------------------------------------------------

    # The receivers an iteration's loop can run in place for, by method
    # name (aot_loop_kind answers which one the receiver is at run time).
    INLINE_KINDS = {
      "each" => [:array, :hash], "each_pair" => [:hash],
      "each_with_index" => [:array], "map" => [:array], "select" => [:array],
      "any?" => [:array], "all?" => [:array], "times" => [:int],
    }.freeze
    LOOP_KIND_C = { array: "AOT_LOOP_ARRAY", hash: "AOT_LOOP_HASH", int: "AOT_LOOP_INT" }.freeze

    # A block whose body the method's C can hold: required parameters
    # only, no nested block, no `return`, no `break` (the Proc path, which
    # the slow branch needs as well, refuses them too), and no begin (its
    # regions and jumps are laid out for a method body or a block cfunc).
    def inline_loop_ok?(block)
      p = block.parameters
      return false unless p.nil? || (p.is_a?(Prism::BlockParametersNode) && p.locals.empty? &&
        (p.parameters.nil? || (p.parameters.rest.nil? && p.parameters.keyword_rest.nil? && p.parameters.block.nil? &&
          p.parameters.optionals.empty? && p.parameters.posts.empty? && p.parameters.keywords.empty? &&
          p.parameters.requireds.all? { |r| r.is_a?(Prism::RequiredParameterNode) })))
      ok = true
      scan = lambda do |node|
        return unless ok && node
        case node
        when Prism::BlockNode, Prism::LambdaNode, Prism::ReturnNode, Prism::BreakNode, Prism::BeginNode,
             Prism::RescueModifierNode, Prism::RetryNode, Prism::RedoNode
          ok = false
          return
        end
        node.compact_child_nodes.each { |c| scan.call(c) }
      end
      scan.call(block.body)
      ok
    end

    # `recv.each { |x| body }` and the other LOOP_METHODS with a block
    # literal: when the receiver is an Array, a Hash or an Integer whose
    # method is still the core one, the loop runs in the method's C and
    # the body runs in place, with the block's parameters and its own
    # locals as C locals of the method and the outer locals as they are
    # (a local the block writes is a cell already, see collect_cells).
    # No Proc, no env, no yield frame. The GC arena is restored after
    # each turn, as the VM's frame pop does. Any other receiver takes
    # the Proc path (block_literal and the aot_each helpers), so the body
    # is emitted once per kind and once as a block cfunc.
    def inline_loop(node, recv, name)
      block = node.block
      r = tmp(:value)
      emit "#{r} = #{box(recv)};"
      t = tmp(:value)
      lk = "ilX#{@inline_seq += 1}"
      @locals["\0#{lk}"] = [lk, :int]
      emit "#{lk} = aot_loop_kind(mrb, #{r}, #{sym(name)});"
      INLINE_KINDS[name].each_with_index do |kind, i|
        emit "#{i.zero? ? 'if' : '} else if'} (#{lk} == #{LOOP_KIND_C[kind]}) {"
        inline_body(kind, name, r, t, block)
      end
      emit "} else {"
      blk = block_literal(node, block)
      emit_funcall(t, Val.new(r, :value), name, [], [], blk, node)
      emit "}"
      Val.new(t, :value)
    end

    # The locals the block itself introduces (depth 0 at a write), to be
    # C locals of their own: a method local of the same name written
    # later is another variable of another kind.
    def block_local_names(block)
      names = []
      scan = lambda do |node|
        return unless node
        case node
        when Prism::LocalVariableWriteNode, Prism::LocalVariableOperatorWriteNode,
             Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode, Prism::LocalVariableTargetNode
          names << node.name.to_s if node.depth.zero?
        end
        node.compact_child_nodes.each { |c| scan.call(c) }
      end
      scan.call(block.body)
      names.uniq
    end

    def inline_body(kind, name, r, t, block)
      n = @inline_seq += 1
      params = block_params(block)
      saved = {}
      fresh = lambda do |ruby_name, k|
        c = "il#{n}_#{ruby_name}"
        saved[ruby_name] = @locals[ruby_name] unless saved.key?(ruby_name)
        @locals[ruby_name] = [c, k]
        @locals["\0#{c}"] = [c, k]
        c
      end
      pcs = params.map { |p| [fresh.call(p[:name], p[:kind]), p[:kind]] }
      block_local_names(block).each { |nm| fresh.call(nm, :value) unless params.any? { |p| p[:name] == nm } }
      i = "ilI#{n}"
      @locals["\0#{i}"] = [i, :int]
      bv = "ilV#{n}"
      @locals["\0#{bv}"] = [bv, :value]
      label = "ilN#{n}"
      bind = lambda do |c, k, src_boxed|
        emit "#{c} = #{k == :value ? src_boxed : "#{PARAM_UNBOX[k]}(mrb, #{src_boxed})"};"
      end
      out = nil
      case kind
      when :array
        if %w[map select].include?(name)
          out = "ilO#{n}"
          @locals["\0#{out}"] = [out, :value]
          emit "#{out} = mrb_ary_new(mrb);"
        end
        emit "#{t} = #{name == 'all?' ? 'mrb_true_value()' : 'mrb_false_value()'};" if %w[any? all?].include?(name)
        emit "for (#{i} = 0; #{i} < RARRAY_LEN(#{r}); #{i}++) {"
        emit "int ilA#{n} = mrb_gc_arena_save(mrb);"
        emit "mrb_value ilE#{n} = RARRAY_PTR(#{r})[#{i}];"
        if name == "each_with_index"
          bind.call(pcs[0][0], pcs[0][1], "ilE#{n}") if pcs[0]
          if pcs[1]
            c, k = pcs[1]
            emit(k == :int ? "#{c} = (aot_int)#{i};" : "#{c} = #{k == :value ? "mrb_int_value(mrb, #{i})" : "#{PARAM_UNBOX[k]}(mrb, mrb_int_value(mrb, #{i}))"};")
          end
          pcs[2..].to_a.each { |c, k| bind.call(c, k, "mrb_nil_value()") }
        elsif pcs.size == 1
          bind.call(pcs[0][0], pcs[0][1], "ilE#{n}")
        elsif pcs.size > 1
          emit "{ mrb_value ilS#{n}[#{pcs.size}];"
          emit "  aot_splat_args(mrb, ilE#{n}, #{pcs.size}, ilS#{n});"
          pcs.each_with_index { |(c, k), j| bind.call(c, k, "ilS#{n}[#{j}]") }
          emit "}"
        end
      when :hash
        kv = "ilK#{n}"
        vv = "ilL#{n}"
        @locals["\0#{kv}"] = [kv, :value]
        @locals["\0#{vv}"] = [vv, :value]
        emit "#{kv} = mrb_hash_keys(mrb, #{r});"
        emit "#{vv} = mrb_hash_values(mrb, #{r});"
        emit "for (#{i} = 0; #{i} < RARRAY_LEN(#{kv}); #{i}++) {"
        emit "int ilA#{n} = mrb_gc_arena_save(mrb);"
        emit "mrb_value ilE#{n} = RARRAY_PTR(#{kv})[#{i}];"
        emit "mrb_value ilW#{n} = #{i} < RARRAY_LEN(#{vv}) ? RARRAY_PTR(#{vv})[#{i}] : mrb_nil_value();"
        if pcs.size == 1
          bind.call(pcs[0][0], pcs[0][1], "mrb_assoc_new(mrb, ilE#{n}, ilW#{n})")
        elsif pcs.size > 1
          bind.call(pcs[0][0], pcs[0][1], "ilE#{n}")
          bind.call(pcs[1][0], pcs[1][1], "ilW#{n}")
          pcs[2..].to_a.each { |c, k| bind.call(c, k, "mrb_nil_value()") }
        end
      when :int
        emit "for (#{i} = 0; #{i} < (aot_int)mrb_integer(#{r}); #{i}++) {"
        emit "int ilA#{n} = mrb_gc_arena_save(mrb);"
        if pcs[0]
          c, k = pcs[0]
          emit(k == :int ? "#{c} = (aot_int)#{i};" : "#{c} = #{k == :value ? "mrb_int_value(mrb, #{i})" : "#{PARAM_UNBOX[k]}(mrb, mrb_int_value(mrb, #{i}))"};")
        end
        pcs[1..].to_a.each { |c, k| bind.call(c, k, "mrb_nil_value()") }
      end
      emit "(void)ilE#{n};" if kind != :int
      emit "(void)ilW#{n};" if kind == :hash
      saved_inline = @inline
      @inline = { label: label, bv: bv, depth: @loop_depth, used: false }
      last = statements(block.body)
      emit "#{bv} = #{box(last)};"
      emit "#{label}:" if @inline[:used]
      @inline = saved_inline
      case name
      when "map" then emit "mrb_ary_push(mrb, #{out}, #{bv});"
      when "select" then emit "if (mrb_test(#{bv})) mrb_ary_push(mrb, #{out}, ilE#{n});"
      when "any?" then emit "if (mrb_test(#{bv})) { #{t} = mrb_true_value(); mrb_gc_arena_restore(mrb, ilA#{n}); break; }"
      when "all?" then emit "if (!mrb_test(#{bv})) { #{t} = mrb_false_value(); mrb_gc_arena_restore(mrb, ilA#{n}); break; }"
      end
      emit "mrb_gc_arena_restore(mrb, ilA#{n});"
      emit "}"
      emit "#{t} = #{out || r};" unless %w[any? all?].include?(name)
      saved.each { |nm, prev| prev ? @locals[nm] = prev : @locals.delete(nm) }
    end

    # ---- block literals ------------------------------------------------

    # A call whose block an env Proc cannot stand in for: the block runs
    # with another self, or becomes a method or a lambda (strict arity, a
    # local `return`).
    BLOCK_REFUSED = %w[instance_eval instance_exec class_eval class_exec module_eval module_exec
                       define_method define_singleton_method lambda].freeze

    # `recv.m(args) { |params| body }`: the body becomes a cfunc of its own
    # and the call site makes a Proc of it with an env
    # (mrb_proc_new_cfunc_with_env) that holds self, each outer local the
    # block reads (a boxed copy, or the cell of a local the block shares
    # with the method, see collect_cells) and the method's own block when
    # the body yields; the block cfunc reads them back at entry. A block
    # these semantics do not cover keeps the def in the irep: a `break`
    # or a `return` (nothing in the C API raises the VM's break), a
    # nested block, and a parameter other than a required one.
    def block_literal(call, block)
      unsupported("a block on #{call.name}", block) if BLOCK_REFUSED.include?(call.name.to_s)
      # mruby-task runs the block of Task.new as an irep (task.c reads
      # proc->body.irep), which a cfunc Proc is not.
      recv = call.receiver
      recv_name = recv.is_a?(Prism::ConstantReadNode) || recv.is_a?(Prism::ConstantPathNode) ? recv.name.to_s : nil
      unsupported("a block on Task.new", block) if call.name == :new && recv_name == "Task"
      unsupported("nested block", block) if @in_block
      params = block_params(block)
      captures = []
      yields = false
      scan = lambda do |node|
        case node
        when nil then return
        when Prism::BlockNode, Prism::LambdaNode then unsupported("nested block", node)
        when Prism::ReturnNode then unsupported("return in a block", node)
        when Prism::LocalVariableReadNode
          captures << node.name.to_s if node.depth > 0
        when Prism::LocalVariableWriteNode, Prism::LocalVariableOperatorWriteNode,
             Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode, Prism::LocalVariableTargetNode
          if node.depth > 0
            # collect_cells made every such local a cell; the capture
            # then carries the cell.
            unsupported("a block writes an outer local (#{node.name})", node) unless cell?(node.name)
            captures << node.name.to_s
          end
        when Prism::YieldNode
          yields = true
        when Prism::CallNode
          yields = true if node.receiver.nil? && node.name == :block_given? && node.arguments.nil?
        end
        node.compact_child_nodes.each { |c| scan.call(c) }
      end
      scan.call(block.body)
      captures.uniq!

      # The env slots after self: each captured C local once (a `&blk`
      # parameter and a `yield` both mean p_blk; a cell local is its
      # cell), a nil-kind local needs none.
      slots = []
      blocals = {}
      captures.each do |name|
        c, k = local(name)
        blocals[name] = local(name)
        next if k == :nil
        slots << [c, k] unless slots.any? { |s| s[0] == c }
      end
      if yields && slots.none? { |s| s[0] == BLOCK_C }
        slots << [BLOCK_C, :value]
        blocals["\0blk"] = [BLOCK_C, :value]
      end
      params.each { |p| blocals[p[:name]] = [p[:c], p[:kind]] unless blocals.key?(p[:name]) }

      fn = "#{@cfunc}_blk#{@block_functions.size + 1}"
      saved = [@out, @locals, @loop_depth, @loops, @in_block, @ud, @outer_loops, @inline]
      @locals = blocals
      @loop_depth = 0
      @loops = []
      @in_block = true
      @inline = nil
      # A begin inside the block is refused (begin_block), so no jump of
      # the block's own is recorded in a begin's userdata.
      @ud = nil
      @outer_loops = 0
      infer_local_kinds(block.body)
      body = +""
      @out = body
      # self and the captures from the env, before anything else runs.
      emit "self = mrb_proc_cfunc_env_get(mrb, 0);"
      emit "(void)self;"
      slots.each_with_index do |(c, k), i|
        emit "#{c} = #{coerce(Val.new("mrb_proc_cfunc_env_get(mrb, #{i + 1})", :value), k)};"
      end
      unless params.empty?
        emit "{ mrb_value bargv[#{params.size}];"
        emit "  aot_block_args(mrb, #{params.size}, bargv);"
        params.each_with_index do |p, i|
          emit "  #{p[:c]} = #{p[:kind] == :value ? "bargv[#{i}]" : "#{PARAM_UNBOX[p[:kind]]}(mrb, bargv[#{i}])"};"
        end
        emit "}"
      end
      last = statements(block.body)
      emit "return #{box(last)};"
      dead = drop_unused_temps(body, [])
      decl = +""
      @locals.each_value do |c, kind|
        next if kind == :nil || dead.include?(c)
        init = kind == :value ? " = mrb_nil_value()" : " = 0"
        decl << "  #{CTYPE[kind]} #{c}#{init};\n"
      end
      @block_functions << "static mrb_value\n#{fn}(mrb_state *mrb, mrb_value self)\n{\n#{decl}#{body}}\n\n"
      @out, @locals, @loop_depth, @loops, @in_block, @ud, @outer_loops, @inline = saved

      # The Proc, at the call site: the env values boxed, in slot order.
      t = tmp(:value)
      vals = ["self"] + slots.map { |c, k| box(Val.new(c, k)) }
      a = "env#{@tmp}"
      emit "{ mrb_value #{a}[] = { #{vals.join(', ')} };"
      emit "  #{t} = mrb_obj_value(mrb_proc_new_cfunc_with_env(mrb, #{fn}, #{vals.size}, #{a})); }"
      Val.new(t, :value)
    end

    # The block's parameters: required ones only, each a boxed argument the
    # block cfunc reads through aot_block_args (one Array over several
    # parameters is splat and a missing argument is nil, as the VM does for
    # a block), unboxed when Spinel proved its kind, as a def's parameter.
    def block_params(block)
      p = block.parameters
      return [] if p.nil?
      unsupported("numbered or `it` block parameter", block) unless p.is_a?(Prism::BlockParametersNode)
      unsupported("block-local variables", block) unless p.locals.empty?
      inner = p.parameters
      return [] if inner.nil?
      if inner.rest || inner.keyword_rest || inner.block || !inner.optionals.empty? || !inner.posts.empty? || !inner.keywords.empty?
        unsupported("block parameter other than a required one", block)
      end
      seen = {}
      inner.requireds.each_with_index.map do |r, i|
        unsupported("destructuring block parameter", r) unless r.is_a?(Prism::RequiredParameterNode)
        kind = @types.scalar(@file, r)
        kind = :value if kind.nil? || param_tested?(r.name)
        name = r.name.to_s
        # `|_, _|`: the second `_` is a slot of its own, read by nothing.
        c = seen[name] ? "b_#{name}_#{i}" : "b_#{name}"
        seen[name] = true
        { name: c == "b_#{name}" ? name : "\0#{c}", c: c, kind: kind }
      end
    end

    # The env holds a copy of the outer local, so the block must see the
    # value the method has at the call and no later one. The method must
    # not write the local after the call, nor anywhere in a loop around it
    # (the callee may keep the Proc and call it later: a timer, a handler).
    # The locals a block shares with the method through a cell: a local
    # a block writes, and a local a block reads that the method writes
    # after the call or inside a loop around it (a copy in the env would
    # be stale when the callee keeps the Proc). The cell is a one-element
    # Array; every read and write of the local, in the method and in the
    # block, goes through it (cell_read, cell_write), so a Proc that
    # outlives the call sees what Ruby's closure sees. A nested block is
    # refused in block_literal and is not looked into here.
    def collect_cells(body)
      @cells = {}
      find_blocks(body, []).each do |block, call, loops|
        reads = []
        writes = []
        scan_outer(block.body, reads, writes)
        writes.each { |name| @cells[name] = true }
        start = call.location.start_offset
        reads.uniq.each do |name|
          stale = local_writes(@def.body, name).any? do |w|
            o = w.location.start_offset
            o > start || loops.any? { |l| l.location.start_offset <= o && w.location.end_offset <= l.location.end_offset }
          end
          @cells[name] = true if stale
        end
      end
    end

    # [block, its call, the while/until nodes around the call] for every
    # block literal in the subtree, outermost blocks only.
    def find_blocks(node, loops, acc = [])
      return acc if node.nil?
      if node.is_a?(Prism::CallNode) && node.block.is_a?(Prism::BlockNode)
        acc << [node.block, node, loops.dup]
        find_blocks(node.receiver, loops, acc)
        find_blocks(node.arguments, loops, acc)
        return acc
      end
      inner = node.is_a?(Prism::WhileNode) || node.is_a?(Prism::UntilNode) ? loops + [node] : loops
      node.compact_child_nodes.each { |c| find_blocks(c, inner, acc) }
      acc
    end

    # The outer locals (depth > 0) a block body reads and writes.
    def scan_outer(node, reads, writes)
      return if node.nil? || node.is_a?(Prism::BlockNode) || node.is_a?(Prism::LambdaNode)
      case node
      when Prism::LocalVariableReadNode
        reads << node.name.to_s if node.depth > 0
      when Prism::LocalVariableWriteNode, Prism::LocalVariableOperatorWriteNode,
           Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode, Prism::LocalVariableTargetNode
        writes << node.name.to_s if node.depth > 0
      end
      node.compact_child_nodes.each { |c| scan_outer(c, reads, writes) }
    end

    # Make the cells at the impl's entry, after the defaults (which read
    # the parameters as plain C locals): a parameter's cell starts with
    # its value, any other local's with nil. From here on the local is
    # its cell.
    def enter_cells
      @cells.each_key do |name|
        src = @locals[name]
        unsupported("a block captures the block parameter (#{name})") if src && src[0] == BLOCK_C
        c = "c_#{name}"
        init = src && @params.any? { |p| p[:c] == src[0] } ? box(Val.new(src[0], src[1])) : "mrb_nil_value()"
        emit "#{c} = aot_cell_new(mrb, #{init});"
        @locals[name] = [c, :value, :cell]
      end
    end

    def cell?(name)
      l = @locals[name.to_s]
      !l.nil? && l[2] == :cell
    end

    def cell_read(node)
      c, = local(node.name)
      t = tmp(:value)
      emit "#{t} = aot_cell_get(mrb, #{c});"
      unbox_proved(node, Val.new(t, :value))
    end

    def cell_write(node, v)
      c, = local(node.name)
      t = tmp(:value)
      emit "#{t} = #{box(v)};"
      emit "aot_cell_set(mrb, #{c}, #{t});"
      unbox_proved(node, Val.new(t, :value))
    end

    # The writes to the method's local `name` in the subtree; inside a
    # block only those that reach out (depth > 0), a block-local of the
    # same name is another variable.
    def local_writes(node, name, acc = [], in_block = false)
      return acc if node.nil?
      in_block = true if node.is_a?(Prism::BlockNode)
      case node
      when Prism::LocalVariableWriteNode, Prism::LocalVariableOperatorWriteNode,
           Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode, Prism::LocalVariableTargetNode
        acc << node if node.name.to_s == name && (!in_block || node.depth > 0)
      end
      node.compact_child_nodes.each { |c| local_writes(c, name, acc, in_block) }
      acc
    end

    # ---- begin/rescue/ensure -------------------------------------------

    # The body of a begin runs as a C function of its own under
    # mrb_protect_error, and so do its rescue clauses, together in a
    # second function that protects the body and matches the exception.
    # mrb_protect_error restores the GC arena after the function it ran,
    # so an object the body made and kept in a C local of the method
    # would be unprotected. The locals are therefore split by region (the
    # body, the clauses, and the rest of the method with the ensure
    # clause): a local that the body or the clauses write and another
    # region references is a cell (as a local a block writes is); a local
    # that one region alone references is a plain C local of that
    # region's function, unless the begin sits in a loop and the region
    # reads the local before it writes it (the value carries over from
    # the last turn, so it is a cell too). Every other local the function
    # reads is a copy in the userdata, taken when the function is called.
    # Records the owned locals of each region in @regions.
    def collect_begin_cells(body)
      refs = []
      local_refs(body, refs, 0)
      find_begins(body, [], []).each do |b, loops|
        regions = { body: [b.statements], rescue: rescue_nodes(b.rescue_clause) }
        by_region = Hash.new { |h, k| h[k] = [] }
        refs.each do |r|
          region = regions.find { |_, nodes| nodes.any? { |n| n && within?(r[:node], n) } }
          by_region[region ? region[0] : :out] << r
        end
        priv = { body: {}, rescue: {} }
        refs.map { |r| r[:name] }.uniq.each do |name|
          uses = by_region.select { |_, rs| rs.any? { |r| r[:name] == name } }.keys
          writes = by_region.select { |_, rs| rs.any? { |r| r[:name] == name && r[:write] } }.keys - [:out]
          next if writes.empty?
          if writes.any? { |w| (uses - [w]).any? }
            @cells[name] = true
            next
          end
          region = writes.first
          first = by_region[region].select { |r| r[:name] == name }.min_by { |r| r[:order] }
          if !loops.empty? && !first[:plain]
            @cells[name] = true
          else
            priv[region][name] = true
          end
        end
        @regions[b] = priv
      end
    end

    # Every reference to a local of the method in the subtree: inside a
    # block only those that reach out to the method's scope. `order` is
    # where the reference takes effect (a plain write at its end, since
    # its right side runs first), `plain` marks a write that reads
    # nothing before it.
    def local_refs(node, acc, depth)
      return if node.nil? || node.is_a?(Prism::DefNode)
      inner = node.is_a?(Prism::BlockNode) || node.is_a?(Prism::LambdaNode) ? depth + 1 : depth
      case node
      when Prism::LocalVariableReadNode
        acc << { name: node.name.to_s, node: node, write: false, order: node.location.start_offset } if node.depth == depth
      when Prism::LocalVariableWriteNode, Prism::LocalVariableTargetNode
        acc << { name: node.name.to_s, node: node, write: true, plain: true, order: node.location.end_offset } if node.depth == depth
      when Prism::LocalVariableOperatorWriteNode, Prism::LocalVariableOrWriteNode, Prism::LocalVariableAndWriteNode
        acc << { name: node.name.to_s, node: node, write: true, plain: false, order: node.location.start_offset } if node.depth == depth
      end
      node.compact_child_nodes.each { |c| local_refs(c, acc, inner) }
    end

    # [BeginNode, the while/until nodes around it] for every begin in the
    # subtree outside a block literal (a begin inside one is refused).
    def find_begins(node, loops, acc)
      return acc if node.nil? || node.is_a?(Prism::BlockNode) || node.is_a?(Prism::LambdaNode) || node.is_a?(Prism::DefNode)
      acc << [node, loops.dup] if node.is_a?(Prism::BeginNode)
      inner = node.is_a?(Prism::WhileNode) || node.is_a?(Prism::UntilNode) ? loops + [node] : loops
      node.compact_child_nodes.each { |c| find_begins(c, inner, acc) }
      acc
    end

    # The rescue clauses from `first` on, in order.
    def rescue_nodes(first)
      nodes = []
      r = first
      while r
        nodes << r
        r = r.subsequent
      end
      nodes
    end

    def within?(node, outer)
      outer.location.start_offset <= node.location.start_offset && node.location.end_offset <= outer.location.end_offset
    end

    # Inside a begin's function a plain (non-cell) local is written only
    # when the region owns it (collect_begin_cells). Anything else is a
    # hole in that analysis, and is refused rather than compiled.
    def check_plain_write(node)
      return unless @ud
      name = node.name.to_s
      unsupported("internal: #{name} written outside its region", node) unless @private[name]
    end

    # `begin body rescue A => e; clause ensure final end`. The body is a
    # C function run under mrb_protect_error; with rescue clauses a
    # second function protects it, matches the exception against each
    # clause's classes in order (aot_rescue_p, StandardError for a bare
    # rescue), binds `=> e`, runs the clause, and re-raises when no
    # clause matches. The method's C fills a userdata struct (self and
    # every local the functions read), calls the function, directly or
    # under mrb_protect_error when there is an ensure clause, runs the
    # ensure clause, re-raises an exception the ensure clause held back,
    # and makes the jump a function recorded (emit_return, emit_break,
    # emit_next). The value of the begin is the body's or the clause's,
    # which mrb_protect_error protected.
    def begin_block(node)
      unsupported("begin inside a block", node) if @in_block
      unsupported("else in a begin", node) if node.else_clause
      priv = @regions[node] || { body: {}, rescue: {} }
      n = (@begins += 1)
      st = "struct #{@cfunc}_ud#{n}"
      members = {}
      saved_jumps = @jumps
      @jumps = {}
      body_fn = "#{@cfunc}_bg#{n}"
      fns = begin_function(body_fn, st, members, priv[:body]) do
        node.statements ? statements(node.statements) : Val.new("mrb_nil_value()", :nil)
      end
      fn = body_fn
      if node.rescue_clause
        fn = "#{@cfunc}_rs#{n}"
        fns << begin_function(fn, st, members, priv[:rescue]) { rescue_clauses(node.rescue_clause, body_fn) }
      end
      jumps = @jumps
      @jumps = saved_jumps
      decl = "#{st} {\n  mrb_value self;\n  int jump;\n" + members.map { |c, k| "  #{CTYPE[k]} #{c};\n" }.join + "};\n\n"
      @block_functions << decl << fns

      u = "ud#{n}"
      t = tmp(:value)
      emit "{ #{st} #{u};"
      emit "  #{u}.self = self;"
      emit "  #{u}.jump = 0;"
      members.each_key { |c| emit "  #{u}.#{c} = #{c};" }
      if node.ensure_clause
        e = tmp(:bool)
        emit "  #{t} = mrb_protect_error(mrb, #{fn}, &#{u}, &#{e});"
        if node.ensure_clause.statements
          v = statements(node.ensure_clause.statements)
          emit "(void)(#{v.code});" unless v.kind == :nil
        end
        emit "  if (#{e}) mrb_exc_raise(mrb, #{t});"
      else
        emit "  #{t} = #{fn}(mrb, &#{u});"
      end
      if jumps[:return]
        emit "  if (#{u}.jump == AOT_JUMP_RETURN) {"
        emit_return(t)
        emit "  }"
      end
      if jumps[:break]
        emit "  if (#{u}.jump == AOT_JUMP_BREAK) {"
        emit_break(node)
        emit "  }"
      end
      if jumps[:next]
        emit "  if (#{u}.jump == AOT_JUMP_NEXT) {"
        emit_next(node)
        emit "  }"
      end
      emit "}"
      Val.new(t, :value)
    end

    # One C function of a begin: `fn(mrb, void *p)` with p the userdata
    # struct `st`. The block emits the statements and returns their
    # value. The function declares each local it names: a cell and a
    # local another region owns come from the userdata (and join
    # `members`, which the method's C fills); a local this region owns
    # and a temp are its own.
    def begin_function(fn, st, members, priv)
      saved = [@out, @loop_depth, @loops, @ud, @outer_loops, @private]
      known = @locals.keys
      @outer_loops += @loop_depth
      @loop_depth = 0
      @loops = []
      @ud = "ud"
      @private = priv
      body = +""
      @out = body
      v = yield
      emit "return #{box(v)};"
      dead = drop_unused_temps(body, [])
      decl = +"  #{st} *ud = (#{st} *)p;\n  mrb_value self = ud->self;\n  (void)self;\n"
      slots = @params.map { |p| [p[:name] || "\0blk", [p[:c], p[:kind]], true] }
      slots += @locals.map { |name, l| [name, l, known.include?(name)] }
      seen = {}
      slots.each do |name, (c, kind, flag), outer|
        next if seen[c] || kind == :nil || dead.include?(c) || body !~ /\b#{c}\b/
        seen[c] = true
        if outer && (flag == :cell || !priv[name])
          members[c] = kind
          decl << "  #{CTYPE[kind]} #{c} = ud->#{c};\n"
        else
          decl << "  #{CTYPE[kind]} #{c}#{kind == :value ? ' = mrb_nil_value()' : ' = 0'};\n"
        end
      end
      (@locals.keys - known).each { |k| @locals.delete(k) }
      @out, @loop_depth, @loops, @ud, @outer_loops, @private = saved
      "static mrb_value\n#{fn}(mrb_state *mrb, void *p)\n{\n#{decl}#{body}}\n\n"
    end

    # The rescue function's statements: the body under mrb_protect_error,
    # then the clauses as an if/else chain over the exception. A clause's
    # classes are evaluated only when the clauses before it did not
    # match, as the VM does.
    def rescue_clauses(first, body_fn)
      r = tmp(:value)
      e = tmp(:bool)
      emit "#{r} = mrb_protect_error(mrb, #{body_fn}, ud, &#{e});"
      emit "if (!#{e}) return #{r};"
      # A `break` of a block the body yielded to unwinds as a break
      # object, which no clause can ask for its class (it has none): on
      # it goes, as the VM's OP_RESCUE lets it.
      emit "if (mrb_break_p(#{r})) mrb_exc_raise(mrb, #{r});"
      arms = rescue_nodes(first).map do |c|
        test_code, test = capture do
          if c.exceptions.empty?
            "mrb_obj_is_kind_of(mrb, #{r}, E_STANDARD_ERROR)"
          else
            c.exceptions.map do |k|
              unsupported("splat in a rescue clause", k) if k.is_a?(Prism::SplatNode)
              "aot_rescue_p(mrb, #{r}, #{box(expr(k))})"
            end.join(" || ")
          end
        end
        body_code, v = capture do
          write_target(c.reference, Val.new(r, :value)) if c.reference
          c.statements ? statements(c.statements) : Val.new("mrb_nil_value()", :nil)
        end
        [test_code, test, body_code, v]
      end
      arms.each_with_index do |(test_code, test, body_code, v), i|
        emit "} else {" if i > 0
        @out << test_code
        emit "if (#{strip_parens(test)}) {"
        @out << body_code
        emit "#{r} = #{box(v)};"
      end
      emit "} else {"
      emit "mrb_exc_raise(mrb, #{r});"
      arms.size.times { emit "}" }
      Val.new(r, :value)
    end

    # The call itself, into `t`: a direct call to the impl when Spinel
    # bound this call to one def of this gem that compiles too and the
    # receiver's method is still that cfunc; a redefinition, an override
    # or another class takes the VM.
    def dispatch(node, t, recv, name, argv, kw, blk)
      target = direct_target(node, argv, kw)
      if target
        @direct_refs << target[:label]
        args = argv.zip(target[:kinds]).map { |v, k| ", #{coerce(v, k)}" }.join
        args += ", mrb_undef_value()" * (target[:kinds].size - argv.size)
        # The keywords by name into the impl's keyword slots, undef for an
        # optional one not given (direct_target saw every required one).
        target[:kw].each do |p|
          given = kw.assoc(p[:name])
          args += given ? ", #{coerce(given[1], p[:kind])}" : ", mrb_undef_value()"
        end
        # The block last. A target that takes none ignores the one given,
        # as the irep would.
        args += ", #{blk ? blk.code : 'mrb_nil_value()'}" if target[:block]
        emit "if (aot_method_is(mrb, #{box(recv)}, #{sym(name)}, #{target[:cfunc]})) {"
        emit "  #{t} = #{target[:cfunc]}_impl(mrb, #{box(recv)}#{args});"
        emit "} else {"
        emit_funcall(t, recv, name, argv, kw, blk, node)
        emit "}"
      else
        emit_funcall(t, recv, name, argv, kw, blk, node)
      end
    end

    # The VM call. With `node`, two calls on an untyped receiver take a
    # helper that answers in place when it can: an iteration with a
    # block (aot_each: a C loop over an Array), and a call without
    # arguments (aot_read0: an attr_reader or a Data member read without
    # a frame).
    def emit_funcall(t, recv, name, argv, kw = [], blk = nil, node = nil)
      return emit_funcall_kw(t, recv, name, argv, kw, blk) unless kw.empty?
      if node && argv.empty?
        if blk && LOOP_METHODS[name]
          emit "#{t} = #{LOOP_METHODS[name]}(mrb, #{box(recv)}, #{blk.code});"
          return
        end
        if blk.nil?
          explicit = !(node.receiver.nil? || node.receiver.is_a?(Prism::SelfNode))
          emit "#{t} = aot_read0(mrb, #{box(recv)}, #{sym(name)}, #{explicit ? 1 : 0});"
          return
        end
      end
      b = blk ? blk.code : nil
      if argv.empty?
        emit "#{t} = #{b ? "mrb_funcall_with_block(mrb, #{box(recv)}, #{sym(name)}, 0, NULL, #{b})" : "mrb_funcall_argv(mrb, #{box(recv)}, #{sym(name)}, 0, NULL)"};"
      else
        a = "argv#{@tmp += 1}"
        emit "{ mrb_value #{a}[] = { #{argv.map { |x| box(x) }.join(', ')} };"
        if b
          emit "  #{t} = mrb_funcall_with_block(mrb, #{box(recv)}, #{sym(name)}, #{argv.size}, #{a}, #{b}); }"
        else
          emit "  #{t} = mrb_funcall_argv(mrb, #{box(recv)}, #{sym(name)}, #{argv.size}, #{a}); }"
        end
      end
    end

    # A call with keywords for the VM. mruby has no C API that passes
    # keywords (mrb_funcall_argv hands a trailing Hash as a positional, and
    # OP_ENTER takes it as one), so aot_funcall_kw goes through a Ruby
    # method of this gem that does `recv.__send__(mid, *args, **kw, &blk)`.
    def emit_funcall_kw(t, recv, name, argv, kw, blk = nil)
      n = (@tmp += 1)
      h = "kwh#{n}"
      b = blk ? blk.code : "mrb_nil_value()"
      emit "{ mrb_value #{h} = mrb_hash_new_capa(mrb, #{kw.size});"
      kw.each { |k, v| emit "  mrb_hash_set(mrb, #{h}, mrb_symbol_value(#{sym(k)}), #{box(v)});" }
      if argv.empty?
        emit "  #{t} = aot_funcall_kw(mrb, #{box(recv)}, #{sym(name)}, 0, NULL, #{h}, #{b}); }"
      else
        a = "argv#{n}"
        emit "  mrb_value #{a}[] = { #{argv.map { |x| box(x) }.join(', ')} };"
        emit "  #{t} = aot_funcall_kw(mrb, #{box(recv)}, #{sym(name)}, #{argv.size}, #{a}, #{h}, #{b}); }"
      end
    end

    # `yield args`: the block the cfunc received, through aot_yield, which
    # raises LocalJumpError as the VM does when there is none. The result
    # stays boxed: the block is the caller's, and what it returns at the
    # call sites Spinel analyzed says nothing about another caller.
    def yield_call(node)
      args = node.arguments ? node.arguments.arguments : []
      args.each do |a|
        unsupported("splat in a yield", node) if a.is_a?(Prism::SplatNode)
        unsupported("keywords in a yield", node) if a.is_a?(Prism::KeywordHashNode)
        unsupported("block-pass in a yield", node) if a.is_a?(Prism::BlockArgumentNode)
      end
      argv = args.map { |a| expr(a) }
      t = tmp(:value)
      if argv.empty?
        emit "#{t} = aot_yield(mrb, #{BLOCK_C}, 0, NULL);"
      else
        a = "argv#{@tmp += 1}"
        emit "{ mrb_value #{a}[] = { #{argv.map { |x| box(x) }.join(', ')} };"
        emit "  #{t} = aot_yield(mrb, #{BLOCK_C}, #{argv.size}, #{a}); }"
      end
      Val.new(t, :value)
    end

    # The compiled def of this gem this call most likely lands on. The
    # runtime test (aot_method_is) keeps the call correct whatever is
    # chosen here, so the choice only decides where the fast path points:
    # Spinel's codegen `callee` when it recorded one (a call Spinel
    # inlined or folded has no record), else a def of the same name in the
    # receiver's class (the def's scope for an implicit or `self` receiver,
    # Spinel's `obj_X` type for an explicit one), else the one def of that
    # name in the gem. A private target only from an implicit or `self`
    # receiver, as the VM allows. Arity must match.
    def direct_target(node, argv, kw = [])
      name = node.name.to_s
      self_call = node.receiver.nil? || node.receiver.is_a?(Prism::SelfNode)
      const_recv = node.receiver.is_a?(Prism::ConstantReadNode) || node.receiver.is_a?(Prism::ConstantPathNode)
      rec = @types.codegen_record(@file, node)
      label =
        if rec && rec["dispatch"] == "direct" && rec["callee"]
          rec["callee"]
        elsif self_call
          # In a singleton method `self` is the class: the call lands on
          # another singleton method ("Marshal.dump_object").
          "#{@scope}#{@singleton ? '.' : '#'}#{name}"
        elsif const_recv
          # `Patch.keep(...)`: a singleton method of the named class
          "#{const_path_name(node.receiver)}.#{name}"
        else
          raw = @types.raw_type(@file, node.receiver)
          raw.to_s.start_with?("obj_") ? "#{raw.sub('obj_', '').gsub('__', '::')}##{name}" : nil
        end
      t = label && (@targets[label] || @targets[label.gsub("::", "__")])
      if t.nil?
        # Spinel names a nested class by its leaf ("Digger#reset") or with
        # "__" for "::"; a method name defined once in the gem is also enough.
        # (A method a module gives the class, `JSON::Common#expect` called
        # from `JSON::Parser`, is found this way too.) A self call in a
        # singleton method looks among the singleton methods instead.
        sep = (@singleton && self_call) || const_recv ? "." : "#"
        cands = @targets.select { |l, _| l.end_with?(sep + name) && !l.include?(sep == "." ? "#" : ".") }
        if cands.size > 1 && label
          leaf = label.split("::").last.split("__").last
          narrowed = cands.select { |l, _| l.end_with?(leaf) }
          cands = narrowed if narrowed.size == 1
        end
        t = cands.size == 1 ? cands.values.first : nil
        label = cands.keys.first if t
      end
      return nil unless t && argv.size.between?(t[:required], t[:kinds].size)
      return nil if t[:private] && !self_call
      # Keywords: every one given must be a parameter of the target and
      # every required one must be given; else the VM raises the error.
      names = (t[:kw] || []).map { |p| p[:name] }
      return nil unless kw.all? { |k, _| names.include?(k) }
      return nil unless (t[:kw] || []).all? { |p| !p[:required] || kw.assoc(p[:name]) }
      t.merge(label: label)
    end

    # "A::B" for a constant receiver; the leaf alone when a part is not a
    # constant (`x::Y`).
    def const_path_name(node)
      return node.name.to_s if node.is_a?(Prism::ConstantReadNode)
      node.full_name
    rescue StandardError
      node.name.to_s
    end

    # `l op r`. Both unboxed: pure C. Otherwise a guarded helper when the
    # operator has one (an Integer/Float fast path, else the VM), its
    # result unboxed when Spinel proved it. With hint: true a nil result
    # means "not an operator; make a normal call".
    def binop(name, l, r, node, hint: false)
      lk = use_kind(l)
      rk = use_kind(r)
      if lk != :value && rk != :value && numeric_binop(name, Val.new("", lk), Val.new("", rk))
        return numeric_binop(name, materialize(l), materialize(r))
      end
      if GUARDED[name]
        t = tmp(:value)
        emit "#{t} = #{GUARDED[name]}(mrb, #{box(l)}, #{box(r)});"
        return unbox_proved(node, Val.new(t, :value))
      end
      return nil if hint
      t = tmp(:value)
      a = "argv#{@tmp}"
      emit "{ mrb_value #{a}[] = { #{box(r)} };"
      emit "  #{t} = mrb_funcall_argv(mrb, #{box(l)}, #{sym(name)}, 1, #{a}); }"
      Val.new(t, :value)
    end

    def numeric_binop(name, l, r)
      return nil if l.kind == :bool || r.kind == :bool || l.kind == :nil || r.kind == :nil
      if l.kind == :int && r.kind == :int
        if INT_BINOPS[name]
          return Val.new("#{INT_BINOPS[name]}(mrb, #{l.code}, #{r.code})", :int)
        elsif INT_BITOPS[name]
          return Val.new("(#{l.code} #{INT_BITOPS[name]} #{r.code})", :int)
        elsif CMP_OPS[name]
          return Val.new("(#{l.code} #{CMP_OPS[name]} #{r.code})", :bool)
        end
        return nil
      end
      lf = coerce(l, :float)
      rf = coerce(r, :float)
      if FLOAT_BINOPS[name]
        Val.new("(#{lf} #{FLOAT_BINOPS[name]} #{rf})", :float)
      elsif name == "%"
        Val.new("aot_float_mod(#{lf}, #{rf})", :float)
      elsif name == "**"
        Val.new("pow(#{lf}, #{rf})", :float)
      elsif CMP_OPS[name]
        Val.new("(#{lf} #{CMP_OPS[name]} #{rf})", :bool)
      end
    end

    def numeric_unop(name, v)
      case [v.kind, name]
      when [:int, "-@"] then Val.new("aot_int_neg(mrb, #{v.code})", :int)
      when [:float, "-@"] then Val.new("(-#{v.code})", :float)
      when [:int, "to_i"] then v
      when [:int, "to_f"] then Val.new("((mrb_float)#{v.code})", :float)
      when [:float, "to_f"] then v
      when [:float, "to_i"] then Val.new("aot_float_to_i(mrb, #{v.code})", :int)
      when [:int, "zero?"] then Val.new("(#{v.code} == 0)", :bool)
      when [:int, "even?"] then Val.new("((#{v.code} & 1) == 0)", :bool)
      when [:int, "odd?"] then Val.new("((#{v.code} & 1) != 0)", :bool)
      when [:int, "abs"] then Val.new("(#{v.code} < 0 ? aot_int_neg(mrb, #{v.code}) : #{v.code})", :int)
      when [:bool, "!"] then Val.new("(!#{v.code})", :bool)
      when [:value, "!"] then Val.new("(!mrb_test(#{v.code}))", :bool)
      when [:nil, "!"] then Val.new("1", :bool)
      when [:int, "!"], [:float, "!"] then Val.new("0", :bool)
      when [:value, "-@"]
        t = tmp(:value)
        emit "#{t} = aot_neg(mrb, #{v.code});"
        Val.new(t, :value)
      end
    end

    # `raise "msg"` and `raise Klass, "msg"` are raised directly; the class
    # is looked up as any constant in the method (lexical scope first, so
    # `raise WebSocketError` inside Net::WebSocket finds it). Every other
    # form (`raise exc_object`, `raise Klass`, `raise Klass, expr`, three
    # arguments) is Kernel#raise's to interpret, through the VM.
    def raise_call(node, args)
      unsupported("raise without arguments", node) if args.empty?
      if args.size == 1 && args[0].is_a?(Prism::StringNode)
        emit "mrb_raise(mrb, E_RUNTIME_ERROR, #{c_string(args[0].unescaped)});"
        return Val.new("mrb_nil_value()", :nil)
      end
      if args.size == 2 && (args[0].is_a?(Prism::ConstantReadNode) || args[0].is_a?(Prism::ConstantPathNode)) && args[1].is_a?(Prism::StringNode)
        k = expr(args[0])
        emit "aot_raise_obj(mrb, #{k.code}, #{c_string(args[1].unescaped)});"
        return Val.new("mrb_nil_value()", :nil)
      end
      argv = args.map { |a| expr(a) }
      a = "argv#{@tmp += 1}"
      emit "{ mrb_value #{a}[] = { #{argv.map { |x| box(x) }.join(', ')} };"
      emit "  mrb_funcall_argv(mrb, self, #{sym('raise')}, #{argv.size}, #{a}); }"
      Val.new("mrb_nil_value()", :nil)
    end
  end
end
