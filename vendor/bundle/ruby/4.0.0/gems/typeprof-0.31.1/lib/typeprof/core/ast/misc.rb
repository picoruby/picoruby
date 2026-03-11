module TypeProf::Core
  class AST
    class StatementsNode < Node
      def initialize(raw_node, lenv, use_result)
        super(raw_node, lenv)
        stmts = raw_node.body
        @stmts = stmts.map.with_index do |n, i|
          if n
            AST.create_node(n, lenv, i == stmts.length - 1 ? use_result : false)
          else
            last = code_range.last
            DummyNilNode.new(TypeProf::CodeRange.new(last, last), lenv)
          end
        end
      end

      attr_reader :stmts

      def subnodes = { stmts: }

      def install0(genv)
        ret = nil

        post_stmts = []

        @stmts.each do |stmt|
          next if stmt.nil?

          if stmt.is_a?(PostExecutionNode)
            post_stmts << stmt
            next
          end

          ret = stmt.install(genv)
        end

        post_stmts.each do |stmt|
          stmt.install(genv)
        end

        if ret
          ret2 = Vertex.new(self)
          @changes.add_edge(genv, ret, ret2)
          ret2
        else
          Source.new(genv.nil_type)
        end
      end

      def narrowings
        @stmts.size == 1 ? @stmts[0].narrowings : super
      end

      def diff(prev_node)
        if prev_node.is_a?(StatementsNode)
          i = 0
          while i < @stmts.size
            @stmts[i].diff(prev_node.stmts[i])
            if !@stmts[i].prev_node
              j1 = @stmts.size - 1
              j2 = prev_node.stmts.size - 1
              while j1 >= i && j2 >= i
                @stmts[j1].diff(prev_node.stmts[j2])
                if !@stmts[j1].prev_node
                  return
                end
                j1 -= 1
                j2 -= 1
              end
              return
            end
            i += 1
          end
          @prev_node = prev_node if i == prev_node.stmts.size
        end
      end
    end

    class MultiWriteNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @value = AST.create_node(raw_node.value, lenv)
        @lefts = raw_node.lefts.map do |raw_lhs|
          AST.create_target_node(raw_lhs, lenv)
        end
        if raw_node.rest
          # TODO: need more complex case handling
          @rest_exist = true
          case raw_node.rest.type
          when :splat_node
            if raw_node.rest.expression
              @rest = AST.create_target_node(raw_node.rest.expression, lenv)
            end
          when :implicit_rest_node
          else
            raise "unexpected rest node: #{raw_node.rest.type}"
          end
        end
        @rights = raw_node.rights.map do |raw_lhs|
          AST.create_target_node(raw_lhs, lenv)
        end
        # TODO: raw_node.rest, raw_node.rights
      end

      attr_reader :value, :lefts, :rest, :rest_exist, :rights

      def subnodes = { value:, lefts:, rest:, rights: }
      def attrs = { rest_exist: }

      def install0(genv)
        value = @value.install(genv)

        @lefts.each {|lhs| lhs.install(genv) }
        @lefts.each {|lhs| lhs.rhs.ret || raise(lhs.rhs.inspect) }
        lefts = @lefts.map {|lhs| lhs.rhs.ret }

        if @rest_exist
          rest_elem = Vertex.new(self)
          if @rest
            @rest.install(genv)
            @rest.rhs.ret || raise(@rest.rhs.inspect)
            @changes.add_edge(genv, Source.new(Type::Instance.new(genv, genv.mod_ary, [rest_elem])), @rest.rhs.ret)
          end
        end

        if @rights
          @rights.each {|lhs| lhs.install(genv) }
          @rights.each {|lhs| lhs.rhs.ret || raise(lhs.rhs.inspect) }
          rights = @rights.map {|rhs| rhs.ret }
        end

        box = @changes.add_masgn_box(genv, value, lefts, rest_elem, rights)
        box.ret
      end

      def retrieve_at(pos, &blk)
        yield self if @var_code_range && @var_code_range.include?(pos)
        super(pos, &blk)
      end
    end

    class MultiTargetNode < Node
      def initialize(raw_node, rhs, lenv)
        super(raw_node, lenv)
        @rhs = rhs
        @lefts = raw_node.lefts.map do |raw_lhs|
          AST.create_target_node(raw_lhs, lenv)
        end
        if raw_node.rest
          case raw_node.rest.type
          when :splat_node
            if raw_node.rest.expression
              @rest = AST.create_target_node(raw_node.rest.expression, lenv)
            end
          when :implicit_rest_node
            # no assignment target
          else
            raise "unexpected rest node in multi_target: #{raw_node.rest.type}"
          end
        end
        @rights = raw_node.rights.map do |raw_rhs|
          AST.create_target_node(raw_rhs, lenv)
        end
      end

      attr_reader :rhs, :lefts, :rest, :rights

      def subnodes = { rhs:, lefts:, rest:, rights: }

      def install0(genv)
        # The rhs should be installed by the parent MultiWriteNode
        # Here we set up the multi-assignment box for nested destructuring
        value = @rhs.install(genv)

        @lefts.each {|lhs| lhs.install(genv) }
        @lefts.each {|lhs| lhs.rhs.ret || raise(lhs.rhs.inspect) }
        lefts = @lefts.map {|lhs| lhs.rhs.ret }

        rest_elem = nil
        if @rest
          rest_elem = Vertex.new(self)
          @rest.install(genv)
          @rest.rhs.ret || raise(@rest.rhs.inspect)
          @changes.add_edge(genv, Source.new(Type::Instance.new(genv, genv.mod_ary, [rest_elem])), @rest.rhs.ret)
        end

        rights = nil
        if @rights && !@rights.empty?
          @rights.each {|rhs| rhs.install(genv) }
          @rights.each {|rhs| rhs.rhs.ret || raise(rhs.rhs.inspect) }
          rights = @rights.map {|rhs| rhs.rhs.ret }
        end

        box = @changes.add_masgn_box(genv, value, lefts, rest_elem, rights)
        box.ret
      end
    end

    class MatchWriteNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @call = AST.create_node(raw_node.call, lenv)
        @targets = raw_node.targets.map do |raw_lhs|
          AST.create_target_node(raw_lhs, lenv)
        end
      end

      attr_reader :call, :targets
      def subnodes = { call:, targets: }

      def install0(genv)
        ret = @call.install(genv)
        @targets.each do |target|
          target.install(genv)
          target.rhs.ret || raise(target.rhs.inspect)
          @changes.add_edge(genv, Source.new(Type::Instance.new(genv, genv.mod_str, [])), target.rhs.ret)
        end
        ret
      end
    end

    class DefinedNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @arg = AST.create_node(raw_node.value, lenv)
      end

      attr_reader :arg

      def subnodes = {} # no arg!

      def install0(genv)
        Source.new(genv.true_type, genv.false_type)
      end
    end

    class SourceEncodingNode < Node
      def install0(genv)
        Source.new(Type::Instance.new(genv, genv.resolve_cpath([:Encoding]), []))
      end
    end

    class SplatNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @expr = AST.create_node(raw_node.expression, lenv)
      end

      attr_reader :expr

      def subnodes = { expr: }

      def mid_code_range = nil

      def install0(genv)
        vtx = @expr.install(genv)

        a_args = ActualArguments.new([], [], nil, nil)
        vtx = @changes.add_method_call_box(genv, vtx, :to_a, a_args, false).ret

        @changes.add_splat_box(genv, vtx).ret
      end
    end

    class ForNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        # XXX: tentative implementation
        # raw_node.index
        @expr = AST.create_node(raw_node.collection, lenv)
        @body = raw_node.statements ? AST.create_node(raw_node.statements, lenv) : DummyNilNode.new(TypeProf::CodeRange.new(code_range.last, code_range.last), lenv)
      end

      attr_reader :expr, :body

      def subnodes = { expr:, body: }

      def install0(genv)
        @expr.install(genv)
        @body.install(genv)
        Source.new(genv.nil_type)
      end
    end

    class FlipFlopNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @e1 = AST.create_node(raw_node.left, lenv)
        @e2 = AST.create_node(raw_node.right, lenv)
      end

      attr_reader :e1, :e2

      def subnodes = { e1:, e2: }

      def install0(genv)
        @e1.install(genv)
        @e2.install(genv)
        Source.new(genv.true_type, genv.false_type)
      end
    end

    class MatchRequiredNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @value = AST.create_node(raw_node.value, lenv)
        @pat = AST.create_pattern_node(raw_node.pattern, lenv)
      end

      attr_reader :value, :pat

      def subnodes = { value:, pat: }

      def install0(genv)
        @value.install(genv)
        @pat.install(genv)
        Source.new(genv.nil_type)
      end
    end

    class MatchPredicateNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @value = AST.create_node(raw_node.value, lenv)
        @pat = AST.create_pattern_node(raw_node.pattern, lenv)
      end

      attr_reader :value, :pat

      def subnodes = { value:, pat: }

      def install0(genv)
        @value.install(genv)
        @pat.install(genv)
        Source.new(genv.true_type, genv.false_type)
      end
    end
  end
end
