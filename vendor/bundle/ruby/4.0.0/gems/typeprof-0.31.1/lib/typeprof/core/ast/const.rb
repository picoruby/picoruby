module TypeProf::Core
  class AST
    class ConstantReadNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        case raw_node.type
        when :constant_read_node, :constant_operator_write_node, :constant_or_write_node, :constant_and_write_node
          @cbase = nil
          @toplevel = false
          @cname = raw_node.name
          @cname_code_range = TypeProf::CodeRange.from_node(raw_node.location)
        when :constant_path_node, :constant_path_target_node
          if raw_node.parent
            @cbase = AST.create_node(raw_node.parent, lenv)
            @toplevel = false
          else
            @cbase = nil
            @toplevel = true
          end
          @cname = raw_node.name
          @cname_code_range = TypeProf::CodeRange.from_node(raw_node.name_loc)
        else
          raise raw_node.type.to_s
        end
        @strict_const_scope = lenv.strict_const_scope
      end

      attr_reader :cname, :cbase, :toplevel, :cname_code_range, :strict_const_scope

      def attrs = { cname:, toplevel:, strict_const_scope: }
      def subnodes = { cbase: }

      def define0(genv)
        if @cbase
          ScopedConstRead.new(@cname, @cbase.define(genv), @strict_const_scope)
        else
          BaseConstRead.new(genv, @cname, @toplevel ? CRef::Toplevel : @lenv.cref, @strict_const_scope)
        end
      end

      def undefine0(genv)
        @static_ret.destroy(genv)
      end

      def install0(genv)
        @cbase.install(genv) if @cbase
        box = @changes.add_const_read_box(genv, @static_ret)
        box.ret
      end
    end

    class ConstantWriteNode < Node
      def initialize(raw_node, rhs, lenv)
        super(raw_node, lenv)
        case raw_node.type
        when :constant_write_node, :constant_target_node, :constant_operator_write_node, :constant_or_write_node, :constant_and_write_node
          # C = expr
          @cpath = nil
          @static_cpath = lenv.cref.cpath + [raw_node.name]
          @cname_code_range = TypeProf::CodeRange.from_node(raw_node.respond_to?(:name_loc) ? raw_node.name_loc : raw_node)
        when :constant_path_write_node, :constant_path_operator_write_node, :constant_path_or_write_node, :constant_path_and_write_node
          # expr::C = expr
          @cpath = AST.create_node(raw_node.target, lenv)
          @static_cpath = AST.parse_cpath(raw_node.target, lenv.cref)
          @cname_code_range = TypeProf::CodeRange.from_node(raw_node.target)
        when :constant_path_target_node
          # expr::C, * = ary
          @cpath = ConstantReadNode.new(raw_node, lenv)
          @static_cpath = AST.parse_cpath(raw_node, lenv.cref)
          @cname_code_range = TypeProf::CodeRange.from_node(raw_node)
        else
          raise
        end
        @rhs = rhs
      end

      attr_reader :cpath, :rhs, :static_cpath, :cname_code_range

      def subnodes = { cpath:, rhs: }
      def attrs = { static_cpath: }

      def define0(genv)
        @cpath.define(genv) if @cpath
        @rhs.define(genv) if @rhs
        if @static_cpath
          mod = genv.resolve_const(@static_cpath)
          mod.add_def(self)
          mod
        else
          nil
        end
      end

      def define_copy(genv)
        if @static_cpath
          mod = genv.resolve_const(@static_cpath)
          mod.add_def(self)
          mod.remove_def(@prev_node)
        end
        super(genv)
      end

      def undefine0(genv)
        if @static_cpath
          genv.resolve_const(@static_cpath).remove_def(self)
        end
        @rhs.undefine(genv) if @rhs
        @cpath.undefine(genv) if @cpath
      end

      def install0(genv)
        @cpath.install(genv) if @cpath
        val = @rhs.install(genv)
        if @static_cpath
          @changes.add_edge(genv, val, @static_ret.vtx)
        end
        val
      end
    end
  end
end
