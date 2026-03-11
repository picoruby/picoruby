module TypeProf::Core
  class AST
    class ModuleBaseNode < Node
      def initialize(raw_node, lenv, raw_cpath, meta, raw_scope, use_result)
        super(raw_node, lenv)

        @cpath = AST.create_node(raw_cpath, lenv)
        @static_cpath = AST.parse_cpath(raw_cpath, lenv.cref)
        @tbl = raw_node.locals

        # TODO: class Foo < Struct.new(:foo, :bar)

        if @static_cpath
          ncref = CRef.new(@static_cpath, meta ? :metaclass : :class, nil, lenv.cref)
          nlenv = LocalEnv.new(@lenv.path, ncref, {}, [])
          @body = raw_scope ? AST.create_node(raw_scope, nlenv, use_result) : DummyNilNode.new(code_range, lenv)
        else
          @body = nil
        end

        @cname_code_range = meta ? nil : TypeProf::CodeRange.from_node(raw_node.constant_path)
        @mod_cdef = nil
      end

      attr_reader :tbl, :cpath, :static_cpath, :cname_code_range, :body

      def subnodes = { cpath:, body: }
      def attrs = { static_cpath:, tbl: }

      def define0(genv)
        @cpath.define(genv)
        if @static_cpath
          mod = genv.resolve_cpath(@static_cpath)
          @mod_cdef = mod.add_module_def(genv, self)
          @body.define(genv)
        else
          kind = self.is_a?(ModuleNode) ? "module" : "class"
          @changes.add_diagnostic(:code_range, "TypeProf cannot analyze a non-static #{ kind }") # warning
          nil
        end
      end

      def define_copy(genv)
        if @static_cpath
          mod = genv.resolve_cpath(@static_cpath)
          @mod_cdef = mod.add_module_def(genv, self)
          mod.remove_module_def(genv, @prev_node)
        end
        super(genv)
      end

      def undefine0(genv)
        if @static_cpath
          mod = genv.resolve_cpath(@static_cpath)
          mod.remove_module_def(genv, self)
          @body.undefine(genv)
        end
        @cpath.undefine(genv)
      end

      def install0(genv)
        @cpath.install(genv)
        if @static_cpath
          @tbl.each {|var| @body.lenv.locals[var] = Source.new(genv.nil_type) }
          @body.lenv.locals[:"*self"] = @body.lenv.cref.get_self(genv)

          mod_val = Source.new(Type::Singleton.new(genv, genv.resolve_cpath(@static_cpath)))
          @changes.add_edge(genv, mod_val, @mod_cdef.vtx)
          ret = Vertex.new(self)
          @changes.add_edge(genv, @body.install(genv), ret)
          ret
        else
          Source.new
        end
      end

      def modified_vars(tbl, vars)
        # skip
      end
    end

    class ModuleNode < ModuleBaseNode
      def initialize(raw_node, lenv, use_result)
        super(raw_node, lenv, raw_node.constant_path, false, raw_node.body, use_result)
      end
    end

    class ClassNode < ModuleBaseNode
      def initialize(raw_node, lenv, use_result)
        super(raw_node, lenv, raw_node.constant_path, false, raw_node.body, use_result)
        raw_superclass = raw_node.superclass
        @superclass_cpath = raw_superclass ? AST.create_node(raw_superclass, lenv) : nil
      end

      attr_reader :superclass_cpath

      def subnodes
        super.merge!({ superclass_cpath: })
      end

      def define0(genv)
        if @static_cpath && @superclass_cpath
          const = @superclass_cpath.define(genv)
          const.followers << genv.resolve_cpath(@static_cpath) if const
        end
        super(genv)
      end

      def undefine0(genv)
        super(genv)
        @superclass_cpath.undefine(genv) if @superclass_cpath
      end

      def install0(genv)
        @superclass_cpath.install(genv) if @superclass_cpath
        super(genv)
      end
    end

    class SingletonClassNode < ModuleBaseNode
      def initialize(raw_node, lenv, use_result)
        super(raw_node, lenv, raw_node.expression, true, raw_node.body, use_result)
      end
    end
  end
end
