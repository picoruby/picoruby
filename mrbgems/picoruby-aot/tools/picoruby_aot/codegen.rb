# Prism AST + Spinel types -> C that calls the mruby API.
#
# One Compiler instance compiles one `def`.
#
# Value kinds: :int (mrb_int), :float (mrb_float), :bool (mrb_bool), :nil
# (no storage) and :value (mrb_value). A value is unboxed when the
# generated C itself produced it (a literal, arithmetic on unboxed
# operands) or when Spinel proved its kind and that no nil reaches it
# (`--emit-types` "nullable":false; an Integer and an Integer? share the
# tag "int"):
#
# - a parameter so proved is read with mrb_get_args "i"/"f"/"b", as a
#   hand-written cfunc reads it (a Float for "i" truncates, anything else
#   raises TypeError), unless the body tests the parameter's class itself,
#   which says the author expects callers outside the analysis;
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
    CTYPE = { int: "mrb_int", float: "mrb_float", bool: "mrb_bool", value: "mrb_value" }.freeze

    # Integer literals the generated C can carry on every target. A 32-bit
    # mruby with word boxing keeps 31-bit fixnums; anything wider would be a
    # heap Integer or a Bignum there, and an unboxed slot has no room.
    INT_LIT_MAX = 2**30 - 1

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

    # Boxed binary operators with a guarded helper (picoruby_aot.h).
    GUARDED = {
      "+" => "aot_add", "-" => "aot_sub", "*" => "aot_mul", "/" => "aot_div", "%" => "aot_mod",
      "<" => "aot_lt", "<=" => "aot_le", ">" => "aot_gt", ">=" => "aot_ge",
      "==" => "aot_eq", "!=" => "aot_neq",
      "&" => "aot_band", "|" => "aot_bor", "^" => "aot_bxor", "<<" => "aot_lshift", ">>" => "aot_rshift",
    }.freeze

    def initialize(typemap, file, def_node, scope:, cfunc_name:, syms: {})
      @types = typemap
      @file = file
      @def = def_node
      @scope = scope
      @cfunc = cfunc_name
      @tmp = 0
      @locals = {}       # ruby name -> [c name, kind]
      @syms = syms       # ruby name -> c static name, shared by the translation unit
      @loop_depth = 0
    end

    def compile
      check_params
      infer_local_kinds(@def.body)
      body = +""
      @out = body
      emit_params
      if @def.body
        last = statements(@def.body)
        emit "return #{box(last)};"
      else
        emit "return mrb_nil_value();"
      end
      head = +""
      head << "static mrb_value\n#{@cfunc}(mrb_state *mrb, mrb_value self)\n{\n"
      @locals.each_value do |c, kind|
        next if kind == :nil
        init = kind == :value ? " = mrb_nil_value()" : " = 0"
        head << "  #{CTYPE[kind]} #{c}#{init};\n"
      end
      head << body << "}\n"
      head
    end

    def self.sym_decls(syms)
      syms.values.map { |c| "static mrb_sym #{c};\n" }.join
    end

    def arity_spec
      "MRB_ARGS_REQ(#{@params.size})"
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

    # ---- params --------------------------------------------------------

    def check_params
      p = @def.parameters
      @params = []
      return unless p
      unsupported("optional parameters") unless p.optionals.empty?
      unsupported("rest parameter") if p.rest
      unsupported("post parameters") unless p.posts.empty?
      unsupported("keyword parameters") unless p.keywords.empty? && p.keyword_rest.nil?
      unsupported("block parameter") if p.block
      p.requireds.each do |r|
        unsupported("destructuring parameter") unless r.is_a?(Prism::RequiredParameterNode)
        c = "p_#{r.name}"
        kind = @types.scalar(@file, r)
        kind = :value if kind.nil? || param_tested?(r.name)
        @params << { name: r.name.to_s, c: c, kind: kind }
        @locals[r.name.to_s] = [c, kind]
      end
    end

    PARAM_FMT = { int: "i", float: "f", bool: "b", value: "o" }.freeze

    def emit_params
      return if @params.empty?
      fmt = @params.map { |p| PARAM_FMT[p[:kind]] }.join
      refs = @params.map { |p| "&#{p[:c]}" }.join(", ")
      emit "mrb_get_args(mrb, #{fmt.inspect}, #{refs});"
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
          if recv.is_a?(Prism::LocalVariableReadNode) && recv.name == name && CLASS_TESTS.include?(node.name.to_s)
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
      when Prism::BlockNode, Prism::LambdaNode
        unsupported("block", node)
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
        return :value if node.block
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
        unsupported("integer literal out of 31-bit range", node) if node.value.abs > INT_LIT_MAX
        Val.new("((mrb_int)#{node.value})", :int)
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
        unsupported("non-empty hash literal", node) unless node.elements.empty?
        t = tmp(:value)
        emit "#{t} = mrb_hash_new(mrb);"
        Val.new(t, :value)
      when Prism::ParenthesesNode
        node.body ? statements(node.body) : Val.new("mrb_nil_value()", :nil)
      when Prism::StatementsNode
        statements(node)
      when Prism::LocalVariableReadNode
        c, k = local(node.name)
        Val.new(c, k)
      when Prism::LocalVariableWriteNode
        c, k = local(node.name)
        v = expr(node.value)
        emit "#{c} = #{coerce(v, k)};"
        Val.new(c, k)
      when Prism::LocalVariableOperatorWriteNode
        c, k = local(node.name)
        v = binop(node.binary_operator.to_s, Val.new(c, k), expr(node.value), node)
        emit "#{c} = #{coerce(v, k)};"
        Val.new(c, k)
      when Prism::LocalVariableOrWriteNode
        c, k = local(node.name)
        unsupported("||= on an unboxed local", node) unless k == :value
        emit "if (!mrb_test(#{c})) {"
        v = expr(node.value)
        emit "#{c} = #{box(v)};"
        emit "}"
        Val.new(c, :value)
      when Prism::InstanceVariableReadNode
        t = tmp(:value)
        emit "#{t} = mrb_iv_get(mrb, self, #{sym(node.name)});"
        unbox_proved(node, Val.new(t, :value))
      when Prism::InstanceVariableWriteNode
        v = expr(node.value)
        t = tmp(:value)
        emit "#{t} = #{box(v)};"
        emit "mrb_iv_set(mrb, self, #{sym(node.name)}, #{t});"
        Val.new(t, :value)
      when Prism::InstanceVariableOperatorWriteNode
        cur = tmp(:value)
        emit "#{cur} = mrb_iv_get(mrb, self, #{sym(node.name)});"
        v = binop(node.binary_operator.to_s, Val.new(cur, :value), expr(node.value), node)
        t = tmp(:value)
        emit "#{t} = #{box(v)};"
        emit "mrb_iv_set(mrb, self, #{sym(node.name)}, #{t});"
        Val.new(t, :value)
      when Prism::InstanceVariableOrWriteNode
        t = tmp(:value)
        emit "#{t} = mrb_iv_get(mrb, self, #{sym(node.name)});"
        emit "if (!mrb_test(#{t})) {"
        v = expr(node.value)
        emit "#{t} = #{box(v)};"
        emit "mrb_iv_set(mrb, self, #{sym(node.name)}, #{t});"
        emit "}"
        Val.new(t, :value)
      when Prism::ConstantReadNode
        t = tmp(:value)
        emit "#{t} = aot_const_get(mrb, self, #{c_string(@scope)}, #{sym(node.name)});"
        Val.new(t, :value)
      when Prism::ConstantPathNode
        path = constant_path(node)
        t = tmp(:value)
        emit "#{t} = mrb_obj_value(aot_resolve_class(mrb, #{c_string(path)}));"
        Val.new(t, :value)
      when Prism::IfNode then conditional(node, false)
      when Prism::UnlessNode then conditional(node, true)
      when Prism::WhileNode then loop(node, false)
      when Prism::UntilNode then loop(node, true)
      when Prism::ReturnNode
        unsupported("return with several values", node) if node.arguments && node.arguments.arguments.size > 1
        v = node.arguments ? expr(node.arguments.arguments.first) : Val.new("mrb_nil_value()", :nil)
        emit "return #{box(v)};"
        Val.new("mrb_nil_value()", :nil)
      when Prism::BreakNode
        unsupported("break outside a while loop", node) if @loop_depth.zero?
        unsupported("break with a value", node) if node.arguments
        emit "break;"
        Val.new("mrb_nil_value()", :nil)
      when Prism::NextNode
        unsupported("next outside a while loop", node) if @loop_depth.zero?
        emit "continue;"
        Val.new("mrb_nil_value()", :nil)
      when Prism::AndNode then logical(node, "&&")
      when Prism::OrNode then logical(node, "||")
      when Prism::CallNode then call(node)
      else
        unsupported("#{node.class.name.split('::').last}", node)
      end
    end

    def constant_path(node)
      parts = []
      n = node
      while n.is_a?(Prism::ConstantPathNode)
        parts.unshift(n.name.to_s)
        n = n.parent
      end
      case n
      when nil then nil
      when Prism::ConstantReadNode then parts.unshift(n.name.to_s)
      else unsupported("dynamic constant path", node)
      end
      parts.join("::")
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
      emit "if (#{cond}) {"
      @out << then_code
      emit "#{result} = #{coerce(v1, kind)};" if result
      emit "} else {"
      @out << else_code
      emit "#{result} = #{coerce(v2, kind)};" if result
      emit "}"
      result ? Val.new(result, kind) : Val.new("mrb_nil_value()", :nil)
    end

    def loop(node, negate)
      unsupported("do-while loop", node) if node.respond_to?(:begin_modifier?) && node.begin_modifier?
      emit "for (;;) {"
      c = expr(node.predicate)
      cond = truthy(c)
      cond = "!(#{cond})" unless negate
      emit "if (#{cond}) break;"
      @loop_depth += 1
      statements(node.statements) if node.statements
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

    def call(node)
      unsupported("block argument", node) if node.block
      unsupported("safe navigation", node) if node.safe_navigation?
      args = node.arguments ? node.arguments.arguments : []
      args.each do |a|
        unsupported("splat argument", node) if a.is_a?(Prism::SplatNode)
        unsupported("keyword arguments", node) if a.is_a?(Prism::KeywordHashNode)
        unsupported("block-pass argument", node) if a.is_a?(Prism::BlockArgumentNode)
      end
      name = node.name.to_s

      return raise_call(node, args) if node.receiver.nil? && name == "raise"

      recv = node.receiver ? expr(node.receiver) : Val.new("self", :value)
      argv = args.map { |a| expr(a) }

      if node.receiver && argv.size == 1
        v = binop(name, recv, argv[0], node, hint: true)
        return v if v
      end
      if node.receiver && argv.empty?
        if recv.kind == :value && recv.proved && numeric_unop(name, Val.new("", recv.proved))
          return numeric_unop(name, materialize(recv))
        end
        v = numeric_unop(name, recv)
        return unbox_proved(node, v) if v
      end

      if node.receiver && @types.raw_type(@file, node.receiver) == "string"
        helper = STR_METHODS[[name, argv.size]]
        if helper
          t = tmp(:value)
          emit "#{t} = #{helper}(mrb, #{([box(recv)] + argv.map { |x| box(x) }).join(', ')});"
          return unbox_proved(node, Val.new(t, :value))
        end
      end

      t = tmp(:value)
      if argv.empty?
        emit "#{t} = mrb_funcall_argv(mrb, #{box(recv)}, #{sym(name)}, 0, NULL);"
      else
        a = "argv#{@tmp}"
        emit "{ mrb_value #{a}[] = { #{argv.map { |x| box(x) }.join(', ')} };"
        emit "  #{t} = mrb_funcall_argv(mrb, #{box(recv)}, #{sym(name)}, #{argv.size}, #{a}); }"
      end
      # `recv.attr = v` and `recv[i] = v` evaluate to v.
      if node.receiver && name.end_with?("=") && !%w[== != <= >= ===].include?(name)
        return argv.last
      end
      unbox_proved(node, Val.new(t, :value))
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
        emit "aot_raise(mrb, \"RuntimeError\", #{c_string(args[0].unescaped)});"
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
