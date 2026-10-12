# For one gem: decide which defs become cfuncs, emit the C, and write the
# Ruby that remains for mrbc with those defs blanked out.

require "fileutils"
require "json"
require "prism"
require_relative "gems"
require_relative "typemap"
require_relative "codegen"

module PicorubyAot
  class Generator
    Entry = Struct.new(:file, :scope, :name, :singleton, :private, :node, :strip_node, :status, :reason, :cfunc)

    def initialize(gem, typemap, exclude: [], include_widened: false)
      @gem = gem
      @types = typemap
      @exclude = exclude
      @include_widened = include_widened
      @entries = []
    end

    attr_reader :entries

    def funcname
      @gem.name.tr("-", "_")
    end

    # Methods that hand control to the task scheduler. A cfunc cannot be
    # switched away from (mruby-task switches VM contexts between
    # instructions, never under a C frame), so a compiled loop around one
    # of these spins forever and a compiled `sleep_ms` returns at once.
    WAIT_NAMES = %w[sleep sleep_ms usleep pass suspend join wait resume].freeze
    WAIT_RECEIVER_TYPES = /\Aobj_Task(__|\z)/

    # Walk every mrblib file and classify every def.
    def collect
      @gem.rbfiles.each do |file|
        src = File.read(file)
        result = Prism.parse(src, filepath: file)
        raise "parse error in #{file}" if result.failure?
        walk_body(result.value.statements, [], file, false)
      end
      exclude_waiting_defs
      self
    end

    # A def waits if it calls a scheduler method, anything on a Task, or
    # (transitively) another def of this gem that waits. Receivers are
    # resolved through Spinel's type when it names a class of this gem.
    def exclude_waiting_defs
      by_label = {}
      @entries.each { |e| by_label["#{e.scope}#{e.singleton ? '.' : '#'}#{e.name}"] = e }
      waits = {}
      calls = {}
      @entries.each do |e|
        calls[e] = []
        collect_calls(e.node.body, calls[e])
      end
      loop do
        changed = false
        @entries.each do |e|
          next if waits[e]
          hit = calls[e].find do |call|
            name = call.name.to_s
            recv = call.receiver
            next true if WAIT_NAMES.include?(name) && (recv.nil? || recv.is_a?(Prism::SelfNode) || recv.is_a?(Prism::ConstantReadNode) || recv.is_a?(Prism::ConstantPathNode) || recv.is_a?(Prism::CallNode))
            next true if recv.is_a?(Prism::ConstantReadNode) && recv.name == :Task
            raw = @types.raw_type(e.file, recv) if recv
            next true if raw && raw =~ WAIT_RECEIVER_TYPES
            target = if recv.nil? || recv.is_a?(Prism::SelfNode)
                       by_label["#{e.scope}##{name}"]
                     elsif raw && raw.start_with?("obj_")
                       by_label["#{raw.sub('obj_', '').gsub('__', '::')}##{name}"]
                     end
            next true if target && waits[target]
            # Receiver of unknown class: if any def of this gem with that
            # name waits, assume this call does too.
            target.nil? && @entries.any? { |o| o.name == name && !o.singleton && waits[o] }
          end
          if hit
            waits[e] = hit
            changed = true
          end
        end
        break unless changed
      end
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
          probe = Compiler.new(@types, e.file, e.node, scope: e.scope, cfunc_name: e.cfunc)
          kinds = probe.param_kinds
          required = probe.required_count
        rescue Unsupported, StandardError
          next # the def will not compile; no one calls it directly
        end
        targets[spinel_label(e)] = { cfunc: e.cfunc, kinds: kinds, required: required, private: e.private }
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
          compiler = Compiler.new(@types, e.file, e.node, scope: e.scope, cfunc_name: e.cfunc, syms: @syms, paths: @paths, targets: targets)
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
          define = e.private ? "mrb_define_private_method" : (e.singleton ? "mrb_define_class_method" : "mrb_define_method")
          # aot_scope_init records the scope's classes for the body's constant
          # reads (a path is initialized once; later calls only re-read it).
          target = e.scope.empty? ? "mrb->object_class" : "aot_scope_init(mrb, &#{Compiler.path_name(@paths, e.scope)})"
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

    def walk_body(stmts, scope, file, private_default)
      return unless stmts
      list = stmts.is_a?(Prism::StatementsNode) ? stmts.body : [stmts]
      private_now = private_default
      list.each do |node|
        case node
        when Prism::ClassNode, Prism::ModuleNode
          name = const_name(node.constant_path)
          inner = name.start_with?("::") ? [name.sub("::", "")] : scope + [name]
          walk_body(node.body, inner, file, false)
        when Prism::DefNode
          add_def(node, node, scope, file, private_now)
        when Prism::CallNode
          if node.receiver.nil? && node.arguments.nil? && %i[private public protected].include?(node.name)
            private_now = (node.name == :private)
          elsif node.receiver.nil? && %i[private public].include?(node.name) && node.arguments &&
                node.arguments.arguments.size == 1 && node.arguments.arguments[0].is_a?(Prism::DefNode)
            add_def(node.arguments.arguments[0], node, scope, file, node.name == :private)
          end
        end
      end
    end

    def add_def(def_node, strip_node, scope, file, is_private)
      singleton = false
      if def_node.receiver
        return unless def_node.receiver.is_a?(Prism::SelfNode)
        singleton = true
      end
      e = Entry.new(file, scope.join("::"), def_node.name.to_s, singleton, is_private, def_node, strip_node, :candidate, nil, nil)
      @entries << e
      label = "#{e.scope}#{singleton ? '.' : '#'}#{e.name}"
      rec = @types.def_record(file, def_node)
      if @exclude.include?(label)
        e.status = :irep
        e.reason = "excluded by spec.aot_exclude"
      elsif rec.nil?
        e.status = :irep
        e.reason = "no type record from spinel"
      elsif rec["widened"] && !@include_widened
        e.status = :irep
        e.reason = "widened to untyped: #{rec['signature']}"
      elsif e.name == "initialize"
        e.status = :irep
        e.reason = "initialize stays in the irep (MVP)"
      end
    end
  end
end
