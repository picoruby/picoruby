module TypeProf::Core
  class AST
    def self.is_a_class(node)
      if node.is_a?(CallNode)
        recv = node.recv
        if recv.is_a?(LocalVariableReadNode)
          if node.positional_args && node.positional_args.size == 1 && node.positional_args[0].static_ret
            # TODO: need static resolusion of a constant
            return [recv.var, node.positional_args[0].static_ret]
          end
        end
      end
      return nil
    end

    # Apply multiple narrowings from the new narrowing system
    def self.with_narrowing(genv, node, lenv, narrowing)
      return yield if narrowing.map.empty?

      # Store original vertices (only for local variables)
      original_vtxs = {}
      narrowing.map.each do |var, narrowing|
        original_vtxs[var] = var.start_with?("@") ? nil : lenv.get_var(var)
      end

      # Apply all narrowings
      narrowing.map.each do |var, narrowing|
        if var.start_with?("@")
          lenv.push_ivar_narrowing(var, narrowing)
        else
          original_vtx = original_vtxs[var]
          narrowed_vtx = original_vtx.new_vertex(genv, node)
          narrowed_vtx = narrowing.narrow(genv, node, narrowed_vtx)
          lenv.set_var(var, narrowed_vtx)
        end
      end

      result = yield

      # Restore original vertices and remove instance variable filters
      original_vtxs.each do |var, original_vtx|
        if var.start_with?("@")
          lenv.pop_ivar_narrowing(var)
        else
          lenv.set_var(var, original_vtx)
        end
      end

      result
    end

    class BranchNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @cond = AST.create_node(raw_node.predicate, lenv)
        @then = raw_node.statements ? AST.create_node(raw_node.statements, lenv) : nil
        else_clause = raw_node.is_a?(Prism::IfNode) ? raw_node.subsequent : raw_node.else_clause
        if else_clause
          else_clause = else_clause.statements if else_clause.type == :else_node
          @else = else_clause ? AST.create_node(else_clause, lenv) : nil
        else
          @else = nil
        end
      end

      attr_reader :cond, :then, :else

      def subnodes = { cond:, then:, else: }

      def install0(genv)
        ret = Vertex.new(self)

        @cond.install(genv)

        vars = []
        @then.modified_vars(@lenv.locals.keys, vars) if @then
        @else.modified_vars(@lenv.locals.keys, vars) if @else
        then_narrowing, else_narrowing = @cond.narrowings
        vars.concat(then_narrowing.map.keys.reject {|var| var.start_with?("@") })
        vars.concat(else_narrowing.map.keys.reject {|var| var.start_with?("@") })
        modified_vtxs = {}
        vars.uniq.each do |var|
          vtx = @lenv.get_var(var)
          nvtx_then = vtx.new_vertex(genv, self)
          nvtx_else = vtx.new_vertex(genv, self)
          modified_vtxs[var] = [nvtx_then, nvtx_else]
        end

        narrowing = self.is_a?(IfNode) ? then_narrowing : else_narrowing
        modified_vtxs.each do |var, (nvtx_then, _)|
          @lenv.set_var(var, nvtx_then)
        end
        then_val = AST.with_narrowing(genv, self, @lenv, narrowing) do
          val = @then ? @then.install(genv) : Source.new(genv.nil_type)
          modified_vtxs.each do |var, ary|
            ary[0] = @lenv.get_var(var)
          end
          val
        end
        @changes.add_edge(genv, then_val, ret)

        narrowing = self.is_a?(IfNode) ? else_narrowing : then_narrowing
        modified_vtxs.each do |var, (_, nvtx_else)|
          @lenv.set_var(var, nvtx_else)
        end
        else_val = AST.with_narrowing(genv, self, @lenv, narrowing) do
          val = @else ? @else.install(genv) : Source.new(genv.nil_type)
          modified_vtxs.each do |var, ary|
            ary[1] = @lenv.get_var(var)
          end
          val
        end
        @changes.add_edge(genv, else_val, ret)

        modified_vtxs.each do |var, (nvtx_then, nvtx_else)|
          nvtx_then = BotFilter.new(genv, self, nvtx_then, then_val).next_vtx
          nvtx_else = BotFilter.new(genv, self, nvtx_else, else_val).next_vtx
          nvtx_join = nvtx_then.new_vertex(genv, self)
          @changes.add_edge(genv, nvtx_else, nvtx_join)
          @lenv.set_var(var, nvtx_join)
        end

        ret
      end
    end

    class IfNode < BranchNode
    end

    class UnlessNode < BranchNode
    end

    class LoopNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @cond = AST.create_node(raw_node.predicate, lenv)
        @body = raw_node.statements ? AST.create_node(raw_node.statements, lenv) : DummyNilNode.new(code_range, lenv)
      end

      attr_reader :cond, :body

      def subnodes = { cond:, body: }

      def install0(genv)
        vars = []
        vars << @cond.var if @cond.is_a?(LocalVariableReadNode)
        @cond.modified_vars(@lenv.locals.keys, vars)
        @body.modified_vars(@lenv.locals.keys, vars)
        vars.uniq!
        old_vtxs = {}
        vars.each do |var|
          vtx = @lenv.get_var(var)
          nvtx = vtx.new_vertex(genv, self)
          old_vtxs[var] = nvtx
          @lenv.set_var(var, nvtx)
        end

        @cond.install(genv)
        if @cond.is_a?(LocalVariableReadNode)
          nvtx_then = NilFilter.new(genv, self, old_vtxs[@cond.var], self.is_a?(UntilNode)).next_vtx
          @lenv.set_var(@cond.var, nvtx_then)
        end

        if @lenv.exist_var?(:"*expected_block_ret")
          expected_block_ret = @lenv.locals[:"*expected_block_ret"]
          @lenv.set_var(:"*expected_block_ret", nil)
        end

        @body.install(genv)

        if expected_block_ret
          @lenv.set_var(:"*expected_block_ret", expected_block_ret)
        end

        vars.each do |var|
          @changes.add_edge(genv, @lenv.get_var(var), old_vtxs[var])
          @lenv.set_var(var, old_vtxs[var])
        end
        if @cond.is_a?(LocalVariableReadNode)
          nvtx_then = NilFilter.new(genv, self, old_vtxs[@cond.var], !self.is_a?(UntilNode)).next_vtx
          @lenv.set_var(@cond.var, nvtx_then)
        end

        Source.new(genv.nil_type)
      end
    end

    class WhileNode < LoopNode
    end

    class UntilNode < LoopNode
    end

    class BreakNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @arg = AST.parse_return_arguments(raw_node, lenv, code_range)
      end

      attr_reader :arg

      def subnodes = { arg: }

      def install0(genv)
        arg = @arg.install(genv)
        @changes.add_edge(genv, arg, @lenv.get_break_vtx)
        Source.new()
      end
    end

    def self.parse_return_arguments(raw_node, lenv, code_range)
      if raw_node.arguments
        elems = raw_node.arguments.arguments
        if elems.one?
          AST.create_node(elems.first, lenv)
        else
          ArrayNode.new(raw_node.arguments, lenv, elems)
        end
      else
        DummyNilNode.new(code_range, lenv)
      end
    end

    class NextNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @arg = AST.parse_return_arguments(raw_node, lenv, code_range)
      end

      attr_reader :arg

      def subnodes = { arg: }

      def install0(genv)
        @arg.install(genv)
        if @lenv.exist_var?(:"*expected_block_ret")
          @lenv.add_next_box(@changes.add_escape_box(genv, @arg.ret))
        end
        Source.new(Type::Bot.new(genv))
      end
    end

    class RedoNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
      end

      def install0(genv)
        # TODO: This should return a bot type
        Source.new()
      end
    end

    class WhenNode < Node
      def initialize(raw_when_node, lenv, pivot_var = nil)
        super(raw_when_node, lenv)
        @conditions = raw_when_node.conditions.map {|cond| AST.create_node(cond, lenv) }
        @body = raw_when_node.statements ? AST.create_node(raw_when_node.statements, lenv) : DummyNilNode.new(code_range, lenv)
        @pivot_var = pivot_var
      end

      attr_reader :conditions, :body, :pivot_var

      def subnodes = { conditions:, body: }

      def install0(genv)
        @conditions.each {|condition| condition.install(genv) }

        # 型絞り込みが必要な場合（pivot_varが設定されている場合）
        if @pivot_var && @lenv.locals.key?(:"*pivot")
          original_vtx = @lenv.locals[:"*pivot"]

          # 複数条件のOR（union）処理
          filtered_vtxs = []

          @conditions.each do |condition|
            if condition.is_a?(ConstantReadNode) && condition.static_ret
              # 各条件に対して独立して型絞り込みを適用
              condition_vtx = original_vtx.new_vertex(genv, self)
              condition_vtx = IsAFilter.new(genv, self, condition_vtx, false, condition.static_ret).next_vtx
              filtered_vtxs << condition_vtx
            end
          end

          # 複数の絞り込み結果をマージして使用
          if !filtered_vtxs.empty?
            merged_vtx = Vertex.new(self)
            filtered_vtxs.each do |vtx|
              @changes.add_edge(genv, vtx, merged_vtx)
            end
            @lenv.set_var(@pivot_var, merged_vtx)
          end
        end

        @body.install(genv)
      end

      # else節での型除外に使用する条件を取得
      def get_exclusion_conditions
        @conditions.select {|condition| condition.is_a?(ConstantReadNode) && condition.static_ret }
                   .map {|condition| condition.static_ret }
      end
    end

    class CaseNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @pivot = raw_node.predicate ? AST.create_node(raw_node.predicate, lenv) : nil

        # pivot変数名を決定
        pivot_var = @pivot.is_a?(LocalVariableReadNode) ? @pivot.var : nil

        @when_nodes = raw_node.conditions.map {|raw_cond| WhenNode.new(raw_cond, lenv, pivot_var) }
        @else_clause = raw_node.else_clause && raw_node.else_clause.statements ? AST.create_node(raw_node.else_clause.statements, lenv) : DummyNilNode.new(code_range, lenv) # TODO: code_range for NilNode
      end

      attr_reader :pivot, :when_nodes, :else_clause

      def subnodes = { pivot:, when_nodes:, else_clause: }

      def install0(genv)
        ret = Vertex.new(self)
        @pivot&.install(genv)

        # case文での型絞り込みを実装
        if @pivot && @pivot.is_a?(LocalVariableReadNode)
          var = @pivot.var
          original_vtx = @lenv.get_var(var)

          # ダミー変数に元の型情報を設定
          @lenv.set_var(:"*pivot", original_vtx)

          # 各when節を実行
          @when_nodes.each do |when_node|
            clause_result = when_node.install(genv)
            @changes.add_edge(genv, clause_result, ret)
            # 元の型に戻す
            @lenv.set_var(var, original_vtx)
          end

          # else節（他のwhen節で除外された後の型）
          filtered_else_vtx = original_vtx.new_vertex(genv, self)
          @when_nodes.each do |when_node|
            when_node.get_exclusion_conditions.each do |static_ret|
              # 各when節の型を除外（negation）
              filtered_else_vtx = IsAFilter.new(genv, self, filtered_else_vtx, true, static_ret).next_vtx
            end
          end
          @lenv.set_var(var, filtered_else_vtx)
          @changes.add_edge(genv, @else_clause.install(genv), ret)
          @lenv.set_var(var, original_vtx)

          # ダミー変数をクリア
          @lenv.locals.delete(:"*pivot")
        else
          # pivotが変数でない場合は従来通り
          @when_nodes.each do |when_node|
            @changes.add_edge(genv, when_node.install(genv), ret)
          end
          @changes.add_edge(genv, @else_clause.install(genv), ret)
        end

        ret
      end
    end

    class CaseMatchNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @pivot = AST.create_node(raw_node.predicate, lenv)
        @patterns = []
        @clauses = []
        raw_node.conditions.each do |raw_cond|
          raise if raw_cond.type != :in_node
          @patterns << AST.create_pattern_node(raw_cond.pattern, lenv)
          @clauses << (raw_cond.statements ? AST.create_node(raw_cond.statements, lenv) : DummyNilNode.new(code_range, lenv)) # TODO: code_range for NilNode
        end
        @else_clause = raw_node.else_clause && raw_node.else_clause.statements ? AST.create_node(raw_node.else_clause.statements, lenv) : nil
      end

      attr_reader :pivot, :patterns, :clauses, :else_clause

      def subnodes = { pivot:, patterns:, clauses:, else_clause: }

      def install0(genv)
        ret = Vertex.new(self)
        @pivot&.install(genv)
        @patterns.zip(@clauses) do |pattern, clause|
          pattern.install(genv)
          @changes.add_edge(genv, clause.install(genv), ret)
        end
        @changes.add_edge(genv, @else_clause.install(genv), ret) if @else_clause
        ret
      end
    end

    class AndNode < Node
      def initialize(raw_node, e1 = nil, raw_e2 = raw_node.right, lenv)
        super(raw_node, lenv)

        @e1 = e1 || AST.create_node(raw_node.left, lenv)
        @e2 = AST.create_node(raw_e2 || raw_node.right, lenv)
      end

      attr_reader :e1, :e2

      def subnodes = { e1:, e2: }

      def install0(genv)
        ret = Vertex.new(self)

        v1 = @e1.install(genv)

        # For AND: if left side is truthy, apply its narrowing to right side
        # Use legacy detect_narrowing for now to maintain compatibility
        then_narrowing, _else_narrowing = @e1.narrowings
        if then_narrowing
          v2 = AST.with_narrowing(genv, self, @lenv, then_narrowing) do
            @e2.install(genv)
          end
        else
          v2 = @e2.install(genv)
        end

        @changes.add_edge(genv, v1, ret)
        @changes.add_edge(genv, v2, ret)

        ret
      end

      def narrowings
        @narrowings ||= begin
          e1_then_narrowing, e1_else_narrowing = @e1.narrowings
          e2_then_narrowing, e2_else_narrowing = @e2.narrowings
          [e1_then_narrowing.and(e2_then_narrowing), e1_else_narrowing.or(e2_else_narrowing)]
        end
      end
    end

    class OrNode < Node
      def initialize(raw_node, e1 = nil, raw_e2 = raw_node.right, lenv)
        super(raw_node, lenv)

        @e1 = e1 || AST.create_node(raw_node.left, lenv)
        @e2 = AST.create_node(raw_e2 || raw_node.right, lenv)
      end

      attr_reader :e1, :e2

      def subnodes = { e1:, e2: }

      def install0(genv)
        ret = Vertex.new(self)

        v1 = @e1.install(genv)
        v1 = NilFilter.new(genv, self, v1, false).next_vtx

        # For OR: if left side is falsy, apply negated narrowing to right side
        # Use legacy detect_narrowing for now to maintain compatibility
        _then_narrowing, else_narrowing = @e1.narrowings
        if else_narrowing
          # For OR: negated narrowing (negate: true)
          v2 = AST.with_narrowing(genv, self, @lenv, else_narrowing) do
            @e2.install(genv)
          end
        else
          v2 = @e2.install(genv)
        end

        @changes.add_edge(genv, v1, ret)
        @changes.add_edge(genv, v2, ret)

        ret
      end

      def narrowings
        @narrowings ||= begin
          e1_then_narrowing, e1_else_narrowing = @e1.narrowings
          e2_then_narrowing, e2_else_narrowing = @e2.narrowings
          [e1_then_narrowing.or(e2_then_narrowing), e1_else_narrowing.and(e2_else_narrowing)]
        end
      end
    end

    class ReturnNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @arg = AST.parse_return_arguments(raw_node, lenv, code_range)
      end

      attr_reader :arg

      def subnodes = { arg: }

      def install0(genv)
        @arg.install(genv)
        e_ret = @lenv.locals[:"*expected_method_ret"]
        @lenv.add_return_box(@changes.add_escape_box(genv, @arg.ret)) if e_ret
        Source.new(Type::Bot.new(genv))
      end
    end

    class RescueNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)

        @exceptions = raw_node.exceptions.map {|raw_cond| AST.create_node(raw_cond, lenv) }
        @statements = AST.create_node(raw_node.statements, lenv) if raw_node.statements
        if raw_node.reference && @statements
          @reference = AST.create_target_node(raw_node.reference, @statements.lenv)
        end
      end

      attr_reader :exceptions, :reference, :statements

      def subnodes = { exceptions:, reference:, statements: }

      def define0(genv)
        @exceptions.each {|exc| exc.define(genv) }
        @reference.define(genv) if @reference
        @statements.define(genv) if @statements
      end

      def undefine0(genv)
        @exceptions.each {|exc| exc.undefine(genv) }
        @reference.undefine(genv) if @reference
        @statements.undefine(genv) if @statements
      end

      def install0(genv)
        cond_vtxs = @exceptions.map do |exc|
          case exc
          when AST::SplatNode
            ary_vtx = exc.expr.install(genv)
            @changes.add_splat_box(genv, ary_vtx).ret
          else
            exc.install(genv)
          end
        end

        if @reference
          @reference.install(genv)
          cond_vtxs.each do |cond_vtx|
            instance_ty_box = @changes.add_instance_type_box(genv, cond_vtx)
            @changes.add_edge(genv, instance_ty_box.ret, @reference.rhs.ret)
          end
        end

        if @statements
          @statements.install(genv)
        else
          Source.new(genv.nil_type)
        end
      end
    end

    class BeginNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @body = raw_node.statements ? AST.create_node(raw_node.statements, lenv) : DummyNilNode.new(code_range, lenv)

        @rescue_clauses = []
        raw_res = raw_node.rescue_clause
        while raw_res
          @rescue_clauses << AST.create_node(raw_res, lenv)
          raw_res = raw_res.subsequent
        end
        @else_clause = AST.create_node(raw_node.else_clause.statements, lenv) if raw_node.else_clause&.statements
        @ensure_clause = AST.create_node(raw_node.ensure_clause.statements, lenv) if raw_node.ensure_clause&.statements
      end

      attr_reader :body, :rescue_clauses, :else_clause, :ensure_clause

      def subnodes = { body:, rescue_clauses:, else_clause:, ensure_clause: }

      def define0(genv)
        @body.define(genv)
        @rescue_clauses.each {|clause| clause.define(genv) }
        @else_clause.define(genv) if @else_clause
        @ensure_clause.define(genv) if @ensure_clause
      end

      def undefine0(genv)
        @body.undefine(genv)
        @rescue_clauses.each {|clause| clause.undefine(genv) }
        @else_clause.undefine(genv) if @else_clause
        @ensure_clause.undefine(genv) if @ensure_clause
      end

      def install0(genv)
        ret = Vertex.new(self)

        vars = []
        @body.modified_vars(@lenv.locals.keys, vars) if @body
        vars.uniq!

        old_vtxs = {}
        vars.each do |var|
          vtx = @lenv.get_var(var)
          old_vtxs[var] = vtx
        end

        @changes.add_edge(genv, @body.install(genv), ret)

        body_vtxs = {}
        vars.each do |var|
          body_vtxs[var] = @lenv.get_var(var)
        end

        clause_vtxs_list = []
        @rescue_clauses.each do |clause|
          vars.each do |var|
            old_vtx = old_vtxs[var]
            nvtx = old_vtx.new_vertex(genv, self)

            @changes.add_edge(genv, body_vtxs[var], nvtx) unless body_vtxs[var] == old_vtxs[var]

            @lenv.set_var(var, nvtx)
          end

          @changes.add_edge(genv, clause.install(genv), ret)

          clause_vtxs_list << {}
          vars.each do |var|
            clause_vtxs_list.last[var] = @lenv.get_var(var)
          end
        end

        if @else_clause
          vars.each do |var|
            @lenv.set_var(var, body_vtxs[var])
          end
          @changes.add_edge(genv, @else_clause.install(genv), ret)
          clause_vtxs_list << {}
          vars.each do |var|
            clause_vtxs_list.last[var] = @lenv.get_var(var)
          end
        end

        if @ensure_clause
          vars.each do |var|
            union_vtx = old_vtxs[var].new_vertex(genv, self)
            @changes.add_edge(genv, body_vtxs[var], union_vtx)
            clause_vtxs_list.each do |clause_vtx|
              @changes.add_edge(genv, clause_vtx[var], union_vtx)
            end
            @lenv.set_var(var, union_vtx)
          end

          @ensure_clause.install(genv)

          clause_vtxs_list << {}
          vars.each do |var|
            clause_vtxs_list.last[var] = @lenv.get_var(var)
          end
        end

        result_vtxs = {}
        vars.each do |var|
          result_vtx = old_vtxs[var].new_vertex(genv, self)
          result_vtxs[var] = result_vtx

          @changes.add_edge(genv, body_vtxs[var], result_vtx) unless body_vtxs[var] == old_vtxs[var]

          clause_vtxs_list.each do |clause_vtx|
            @changes.add_edge(genv, clause_vtx[var], result_vtx)
          end
        end

        vars.each do |var|
          @lenv.set_var(var, result_vtxs[var])
        end

        ret
      end
    end

    class RetryNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
      end

      def install0(genv)
        Source.new(Type::Bot.new(genv))
      end
    end

    class RescueModifierNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @expression = AST.create_node(raw_node.expression, lenv)
        @rescue_expression = AST.create_node(raw_node.rescue_expression, lenv)
      end

      attr_reader :expression, :rescue_expression

      def subnodes = { expression:, rescue_expression: }

      def install0(genv)
        ret = Vertex.new(self)
        @changes.add_edge(genv, @expression.install(genv), ret)
        @changes.add_edge(genv, @rescue_expression.install(genv), ret)
        ret
      end
    end
  end
end
