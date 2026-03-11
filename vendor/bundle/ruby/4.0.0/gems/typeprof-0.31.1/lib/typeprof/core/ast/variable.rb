module TypeProf::Core
  class AST
    class LocalVariableReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
      end

      attr_reader :var

      def attrs = { var: }

      def install0(genv)
        @lenv.get_var(@var)
      end

      def retrieve_at(pos)
        yield self if code_range.include?(pos)
      end

      def narrowings
        @narrowings ||= [
          Narrowing.new({ @var => Narrowing::NilConstraint.new(false) }),
          Narrowing.new({ @var => Narrowing::NilConstraint.new(true) }),
        ]
      end
    end

    class ItLocalVariableReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
      end

      def attrs = {}

      def install0(genv)
        @lenv.get_var(:it)
      end

      def retrieve_at(pos)
        yield self if code_range.include?(pos)
      end
    end

    class LocalVariableWriteNode < Node
      def initialize(raw_node, rhs, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
        @var_code_range = TypeProf::CodeRange.from_node(raw_node.respond_to?(:name_loc) ? raw_node.name_loc : raw_node)
        @rhs = rhs
      end

      attr_reader :var, :var_code_range, :rhs

      def subnodes = { rhs: }
      def attrs = { var: }

      def install0(genv)
        val = @rhs.install(genv)

        vtx = @lenv.new_var(@var, self)
        @changes.add_edge(genv, val, vtx)
        val
      end

      def retrieve_at(pos, &blk)
        yield self if @var_code_range && @var_code_range.include?(pos)
        super(pos, &blk)
      end

      def modified_vars(tbl, vars)
        vars << self.var if tbl.include?(self.var)
      end
    end

    class InstanceVariableReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
      end

      attr_reader :var

      def attrs = { var: }

      def install0(genv)
        case @lenv.cref.scope_level
        when :class, :instance
          box = @changes.add_ivar_read_box(genv, lenv.cref.cpath, lenv.cref.scope_level == :class, @var)
          @lenv.apply_ivar_narrowing(genv, self, @var, box.ret)
        else
          Source.new()
        end
      end

      def retrieve_at(pos)
        yield self if code_range.include?(pos)
      end

      def narrowings
        @narrowings ||= [
          Narrowing.new({ @var => Narrowing::NilConstraint.new(false) }),
          Narrowing.new({ @var => Narrowing::NilConstraint.new(true) }),
        ]
      end
    end

    class InstanceVariableWriteNode < Node
      def initialize(raw_node, rhs, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
        @var_code_range = TypeProf::CodeRange.from_node(raw_node.respond_to?(:name_loc) ? raw_node.name_loc : raw_node)
        @rhs = rhs
      end

      attr_reader :var, :var_code_range, :rhs

      def subnodes = { rhs: }
      def attrs = { var: }

      def define0(genv)
        @rhs.define(genv) if @rhs
        case @lenv.cref.scope_level
        when :class, :instance
          val = genv.resolve_ivar(@lenv.cref.cpath, @lenv.cref.scope_level == :class, @var)
          val.add_def(self)
          val
        else
          # TODO: warn
          nil
        end
      end

      def define_copy(genv)
        case @lenv.cref.scope_level
        when :class, :instance
          val = genv.resolve_ivar(@lenv.cref.cpath, @lenv.cref.scope_level == :class, @var)
          val.add_def(self)
          val.remove_def(@prev_node)
        end
        super(genv)
      end

      def undefine0(genv)
        case @lenv.cref.scope_level
        when :class, :instance
          val = genv.resolve_ivar(@lenv.cref.cpath, @lenv.cref.scope_level == :class, @var)
          val.remove_def(self)
        end
        @rhs.undefine(genv) if @rhs
      end

      def install0(genv)
        val = @rhs.install(genv)
        case @lenv.cref.scope_level
        when :class, :instance
          @changes.add_ivar_read_box(genv, @lenv.cref.cpath, @lenv.cref.scope_level == :class, @var)
          val = val.new_vertex(genv, self) # avoid multi-edge from val to static_ret.vtx
          @changes.add_edge(genv, val, @static_ret.vtx)
        end
        val
      end

      def retrieve_at(pos, &blk)
        yield self if @var_code_range && @var_code_range.include?(pos)
        super(pos, &blk)
      end
    end

    class GlobalVariableReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
      end

      attr_reader :var

      def attrs = { var: }

      def install0(genv)
        box = @changes.add_gvar_read_box(genv, @var)
        box.ret
      end

      def retrieve_at(pos)
        yield self if code_range.include?(pos)
      end
    end

    class GlobalVariableWriteNode < Node
      def initialize(raw_node, rhs, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
        @var_code_range = TypeProf::CodeRange.from_node(raw_node.respond_to?(:name_loc) ? raw_node.name_loc : raw_node)
        @rhs = rhs
      end

      attr_reader :var, :var_code_range, :rhs

      def subnodes = { rhs: }
      def attrs = { var: }

      def define0(genv)
        @rhs.define(genv) if @rhs
        val = genv.resolve_gvar(@var)
        val.add_def(self)
        val
      end

      def define_copy(genv)
        val = genv.resolve_gvar(@var)
        val.add_def(self)
        val.remove_def(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        val = genv.resolve_gvar(@var)
        val.remove_def(self)
        @rhs.undefine(genv) if @rhs
      end

      def install0(genv)
        val = @rhs.install(genv)
        val = val.new_vertex(genv, self) # avoid multi-edge from val to static_ret.vtx
        @changes.add_edge(genv, val, @static_ret.vtx)
        val
      end

      def retrieve_at(pos, &blk)
        yield self if @var_code_range && @var_code_range.include?(pos)
        super(pos, &blk)
      end
    end

    class AliasGlobalVariableNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        # XXX: Who use this? I want to hard-code English.rb
      end

      def install0(genv)
        Source.new(genv.nil_type)
      end
    end

    class PostExecutionNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @body = raw_node.statements ? AST.create_node(raw_node.statements, lenv) : DummyNilNode.new(TypeProf::CodeRange.new(code_range.last, code_range.last), lenv)
      end

      attr_reader :body

      def subnodes = { body: }

      def install0(genv)
        @body.install(genv)
        Source.new(genv.nil_type)
      end
    end

    class ClassVariableWriteNode < Node
      def initialize(raw_node, rhs, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
        @var_code_range = TypeProf::CodeRange.from_node(raw_node.respond_to?(:name_loc) ? raw_node.name_loc : raw_node)
        @rhs = rhs
      end

      attr_reader :var, :var_code_range, :rhs

      def subnodes = { rhs: }
      def attrs = { var: }

      def define0(genv)
        @rhs.define(genv) if @rhs
        mod = genv.resolve_cvar(@lenv.cref.cpath, @var)
        mod.add_def(self)
        mod
      end

      def define_copy(genv)
        mod = genv.resolve_cvar(@lenv.cref.cpath, @var)
        mod.add_def(self)
        mod.remove_def(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        mod = genv.resolve_cvar(@lenv.cref.cpath, @var)
        mod.remove_def(self)
        @rhs.undefine(genv) if @rhs
      end

      def install0(genv)
        @changes.add_cvar_read_box(genv, @lenv.cref.cpath, @var)
        val = @rhs.install(genv)
        val = val.new_vertex(genv, self) # avoid multi-edge from val to static_ret.vtx
        @changes.add_edge(genv, val, @static_ret.vtx)
        val
      end

      def retrieve_at(pos, &blk)
        yield self if @var_code_range && @var_code_range.include?(pos)
        super(pos, &blk)
      end
    end

    class ClassVariableReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @var = raw_node.name
      end

      attr_reader :var

      def attrs = { var: }

      def install0(genv)
        box = @changes.add_cvar_read_box(genv, lenv.cref.cpath, @var)
        @lenv.apply_ivar_narrowing(genv, self, @var, box.ret)
      end

      def retrieve_at(pos)
        yield self if code_range.include?(pos)
      end
    end

    class RegexpReferenceReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @var = raw_node.type == :back_reference_read_node ? :"$&" : :"$#{raw_node.number}"
      end

      attr_reader :var

      def attrs = { var: }

      def install0(genv)
        Source.new(genv.str_type)
      end
    end
  end
end
