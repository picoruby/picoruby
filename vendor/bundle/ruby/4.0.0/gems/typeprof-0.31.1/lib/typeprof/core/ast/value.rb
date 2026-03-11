module TypeProf::Core
  class AST
    def self.create_part_node(raw_part, lenv)
      case raw_part.type
      when :string_node
        AST.create_node(raw_part, lenv)
      when :embedded_statements_node
        raw_part.statements ? AST.create_node(raw_part.statements, lenv) : DummyNilNode.new(TypeProf::CodeRange.from_node(raw_part), lenv)
      when :embedded_variable_node
        AST.create_node(raw_part.variable, lenv)
      else
        raise "unknown symbol part: #{ raw_part.type }"
      end
    end

    class SelfNode < Node
      def install0(genv)
        @lenv.get_var(:"*self")
      end
    end

    class LiteralNode < Node
      def initialize(raw_node, lenv, lit)
        super(raw_node, lenv)
        @lit = lit
      end

      attr_reader :lit

      def attrs = { lit: }

      def install0(genv)
        raise "not supported yet: #{ @lit.inspect }"
      end
    end

    class NilNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, nil)
      end

      def install0(genv) = Source.new(genv.nil_type)
    end

    class TrueNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, true)
      end

      def install0(genv) = Source.new(genv.true_type)
    end

    class FalseNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, false)
      end

      def install0(genv) = Source.new(genv.false_type)
    end

    class IntegerNode < LiteralNode
      def initialize(raw_node, lenv, lit = raw_node.slice)
        super(raw_node, lenv, Integer(lit))
      end

      def install0(genv) = Source.new(genv.int_type)
    end

    class FloatNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, Float(raw_node.slice))
      end

      def install0(genv) = Source.new(genv.float_type)
    end

    class RationalNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, raw_node.slice.to_r)
      end

      def install0(genv) = Source.new(genv.rational_type)
    end

    class ComplexNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, raw_node.slice.to_c)
      end

      def install0(genv) = Source.new(genv.complex_type)
    end

    class SymbolNode < LiteralNode
      def initialize(raw_node, lenv)
        super(raw_node, lenv, raw_node.value.to_sym)
      end

      def install0(genv) = Source.new(Type::Symbol.new(genv, @lit))
    end

    class InterpolatedSymbolNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @parts = raw_node.parts.map do |raw_part|
          AST.create_part_node(raw_part, lenv)
        end
      end

      attr_reader :parts

      def subnodes = { parts: }

      def install0(genv)
        @parts.each do |subnode|
          subnode.install(genv)
        end
        Source.new(genv.symbol_type)
      end
    end

    class StringNode < LiteralNode
      def initialize(raw_node, lenv, content)
        super(raw_node, lenv, content)
      end

      def install0(genv) = Source.new(genv.str_type)
    end

    class InterpolatedStringNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @parts = []

        queue = raw_node.parts.dup

        until queue.empty?
          raw_part = queue.shift

          if raw_part.type == :interpolated_string_node
            queue.unshift(*raw_part.parts)
          else
            @parts << AST.create_part_node(raw_part, lenv)
          end
        end
      end

      attr_reader :parts

      def subnodes = { parts: }

      def install0(genv)
        @parts.each do |subnode|
          subnode.install(genv)
        end
        Source.new(genv.str_type)
      end
    end

    class RegexpNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
      end

      def install0(genv) = Source.new(genv.regexp_type)
    end

    class InterpolatedRegexpNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @parts = raw_node.parts.map do |raw_part|
          AST.create_part_node(raw_part, lenv)
        end
      end

      attr_reader :parts

      def subnodes = { parts: }

      def install0(genv)
        @parts.each do |subnode|
          subnode.install(genv)
        end
        Source.new(genv.regexp_type)
      end
    end

    class MatchLastLineNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
      end

      def install0(genv) = Source.new(genv.true_type, genv.false_type)
    end

    class InterpolatedMatchLastLineNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @parts = raw_node.parts.map do |raw_part|
          AST.create_part_node(raw_part, lenv)
        end
      end

      attr_reader :parts

      def subnodes = { parts: }

      def install0(genv)
        @parts.each do |subnode|
          subnode.install(genv)
        end
        Source.new(genv.true_type, genv.false_type)
      end
    end

    class RangeNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @begin = raw_node.left ? AST.create_node(raw_node.left, lenv) : DummyNilNode.new(raw_node, lenv)
        @end = raw_node.right ? AST.create_node(raw_node.right, lenv) : DummyNilNode.new(raw_node, lenv)
      end

      attr_reader :begin, :end

      def subnodes = { begin:, end: }

      def install0(genv)
        elem = Vertex.new(self)
        @changes.add_edge(genv, @begin.install(genv), elem)
        @changes.add_edge(genv, @end.install(genv), elem)
        Source.new(genv.gen_range_type(elem))
      end
    end

    class ArrayNode < Node
      def initialize(raw_node, lenv, elems = raw_node.elements)
        super(raw_node, lenv)
        @elems = elems.map {|n| AST.create_node(n, lenv) }
        @splat = @elems.any? {|e| e.is_a?(SplatNode) }
      end

      attr_reader :elems, :splat

      def subnodes = { elems: }
      def attrs = { splat: }

      def install0(genv)
        elems = @elems.map {|e| e.install(genv).new_vertex(genv, self) }
        unified_elem = Vertex.new(self)
        elems.each {|vtx| @changes.add_edge(genv, vtx, unified_elem) }
        base_ty = genv.gen_ary_type(unified_elem)
        if @splat
          Source.new(base_ty)
        else
          Source.new(Type::Array.new(genv, elems, base_ty))
        end
      end
    end

    class HashNode < Node
      def initialize(raw_node, lenv, keywords)
        super(raw_node, lenv)
        @keys = []
        @vals = []
        @keywords = keywords
        @splat = false

        raw_node.elements.each do |raw_elem|
          # TODO: Support :assoc_splat_node
          case raw_elem.type
          when :assoc_node
            @keys << AST.create_node(raw_elem.key, lenv)
            @vals << AST.create_node(raw_elem.value, lenv)
          when :assoc_splat_node
            @keys << nil
            if raw_elem.value
              @vals << AST.create_node(raw_elem.value, lenv)
            else
              @vals << DummyNilNode.new(code_range, lenv)
            end
            @splat = true
          else
            raise "unknown hash elem: #{ raw_elem.type }"
          end
        end
      end

      attr_reader :keys, :vals, :splat, :keywords

      def subnodes = { keys:, vals: }
      def attrs = { splat:, keywords: }

      def install0(genv)
        unified_key = Vertex.new(self)
        unified_val = Vertex.new(self)
        literal_pairs = {}
        @keys.zip(@vals) do |key, val|
          if key
            k = key.install(genv).new_vertex(genv, self)
            v = val.install(genv).new_vertex(genv, self)
            @changes.add_edge(genv, k, unified_key)
            @changes.add_edge(genv, v, unified_val)
            literal_pairs[key.lit] = v if key.is_a?(SymbolNode)
          else
            if val.is_a?(DummyNilNode)
              h = @lenv.get_var(:"**anonymous_keyword")
            else
              h = val.install(genv)
            end
            # TODO: do we want to call to_hash on h?
            @changes.add_hash_splat_box(genv, h, unified_key, unified_val)
          end
        end
        if @splat
          Source.new(genv.gen_hash_type(unified_key, unified_val))
        else
          Source.new(Type::Hash.new(genv, literal_pairs, genv.gen_hash_type(unified_key, unified_val)))
        end
      end
    end

    class LambdaNode < Node
      def install0(genv)
        Source.new(genv.proc_type)
      end
    end
  end
end
