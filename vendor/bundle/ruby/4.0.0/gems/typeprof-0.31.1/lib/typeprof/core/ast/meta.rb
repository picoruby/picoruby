module TypeProf::Core
  class AST
    class IncludeMetaNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        # TODO: error for splat
        @args = raw_node.arguments.arguments.map do |raw_arg|
          next if raw_arg.is_a?(Prism::SplatNode)
          lenv.use_strict_const_scope do
            AST.create_node(raw_arg, lenv)
          end
        end.compact
        # TODO: error for non-LIT
        # TODO: fine-grained hover
      end

      attr_reader :args

      def subnodes = { args: }

      def define0(genv)
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        @args.each do |arg|
          arg.define(genv)
          if arg.static_ret
            arg.static_ret.followers << mod
            mod.add_include_def(genv, arg)
          end
        end
      end

      def undefine0(genv)
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        @args.each do |arg|
          if arg.static_ret
            mod.remove_include_def(genv, arg)
          end
          arg.undefine(genv)
        end
        super(genv)
      end

      def install0(genv)
        @args.each {|arg| arg.install(genv) }
        Source.new
      end
    end

    class AttrReaderMetaNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @args = []
        raw_node.arguments.arguments.each do |raw_arg|
          @args << raw_arg.value.to_sym if raw_arg.type == :symbol_node
        end
        # TODO: error for non-LIT
        # TODO: fine-grained hover
      end

      attr_reader :args

      def attrs = { args: }

      def req_positionals = []
      def opt_positionals = []
      def post_positionals = []
      def rest_positionals = nil
      def req_keywords = []
      def opt_keywords = []
      def rest_keywords = nil

      def mname_code_range(name)
        idx = @args.index(name.to_sym) # TODO: support string args
        node = @raw_node.arguments.arguments[idx].location
        TypeProf::CodeRange.from_node(node)
      end

      def install0(genv)
        @args.each do |arg|
          ivar_name = :"@#{ arg }"
          ivar_box = @changes.add_ivar_read_box(genv, @lenv.cref.cpath, false, ivar_name)
          ret_box = @changes.add_escape_box(genv, ivar_box.ret)
          @changes.add_method_def_box(genv, @lenv.cref.cpath, false, arg, FormalArguments::Empty, [ret_box])
        end
        Source.new
      end
    end

    class AttrAccessorMetaNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @args = []
        raw_node.arguments.arguments.each do |raw_arg|
          @args << raw_arg.value.to_sym if raw_arg.type == :symbol_node
        end
        # TODO: error for non-LIT
        # TODO: fine-grained hover
      end

      attr_reader :args

      def attrs = { args: }

      def mname_code_range(name)
        idx = @args.index(name.to_sym) # TODO: support string args
        node = @raw_node.arguments.arguments[idx].location
        TypeProf::CodeRange.from_node(node)
      end

      def define0(genv)
        @args.map do |arg|
          mod = genv.resolve_ivar(lenv.cref.cpath, false, :"@#{ arg }")
          mod.add_def(self)
          mod
        end
      end

      def define_copy(genv)
        @args.map do |arg|
          mod = genv.resolve_ivar(lenv.cref.cpath, false, :"@#{ arg }")
          mod.add_def(self)
          mod.remove_def(@prev_node)
          mod
        end
        super(genv)
      end

      def undefine0(genv)
        @args.each do |arg|
          mod = genv.resolve_ivar(lenv.cref.cpath, false, :"@#{ arg }")
          mod.remove_def(self)
        end
      end

      def install0(genv)
        @args.zip(@static_ret) do |arg, ive|
          ivar_box = @changes.add_ivar_read_box(genv, @lenv.cref.cpath, false, :"@#{ arg }")
          ret_box = @changes.add_escape_box(genv, ivar_box.ret)
          @changes.add_method_def_box(genv, @lenv.cref.cpath, false, arg, FormalArguments::Empty, [ret_box])

          vtx = Vertex.new(self)
          @changes.add_edge(genv, vtx, ive.vtx)
          f_args = FormalArguments.new([vtx], [], nil, [], [], [], nil, nil)
          @changes.add_method_def_box(genv, @lenv.cref.cpath, false, :"#{ arg }=", f_args, [ret_box])
        end
        Source.new
      end
    end
  end
end
