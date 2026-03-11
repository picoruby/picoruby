module TypeProf::Core
  class AST
    def self.resolve_rbs_name(name, lenv)
      if name.namespace.absolute?
        name.namespace.path + [name.name]
      else
        lenv.cref.cpath + name.namespace.path + [name.name]
      end
    end

    class SigModuleBaseNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @cpath = AST.resolve_rbs_name(raw_decl.name, lenv)
        # TODO: decl.type_params
        # TODO: decl.super_class.args
        ncref = CRef.new(@cpath, :class, nil, lenv.cref)
        nlenv = LocalEnv.new(@lenv.path, ncref, {}, [])
        @members = raw_decl.members.map do |member|
          AST.create_rbs_member(member, nlenv)
        end.compact
        # TODO?: param.variance, param.unchecked, param.upper_bound
        @params = raw_decl.type_params.map {|param| param.name }
      end

      attr_reader :cpath, :members, :params

      def subnodes = { members: }
      def attrs = { cpath:, params: }

      def define0(genv)
        @members.each do |member|
          member.define(genv)
        end
        mod = genv.resolve_cpath(@cpath)
        { module: mod.add_module_decl(genv, self) }
      end

      def define_copy(genv)
        mod = genv.resolve_cpath(@cpath)
        mod.add_module_decl(genv, self)
        mod.remove_module_decl(genv, @prev_node)
        super(genv)
      end

      def undefine0(genv)
        mod = genv.resolve_cpath(@cpath)
        mod.remove_module_decl(genv, self)
        @members.each do |member|
          member.undefine(genv)
        end
      end

      def install0(genv)
        mod_val = Source.new(Type::Singleton.new(genv, genv.resolve_cpath(@cpath)))
        @changes.add_edge(genv, mod_val, @static_ret[:module].vtx)
        @members.each do |member|
          member.install(genv)
        end
        Source.new
      end
    end

    class SigModuleNode < SigModuleBaseNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @self_types = []
        @self_type_args = []
        raw_decl.self_types.each do |self_type|
          name = self_type.name
          cpath = name.namespace.path + [self_type.name.name]
          toplevel = name.namespace.absolute?
          @self_types << [cpath, toplevel]
          @self_type_args << self_type.args.map {|arg| AST.create_rbs_type(arg, lenv) }
        end
      end

      attr_reader :self_types, :self_type_args

      def subnodes
        super.merge!({ self_type_args: })
      end
      def attrs
        super.merge!({ self_types: })
      end

      def define0(genv)
        static_ret = super(genv)
        static_ret[:self_types] = self_types = []
        @self_types.zip(@self_type_args) do |(cpath, toplevel), args|
          args.each {|arg| arg.define(genv) }
          const_read = BaseConstRead.new(genv, cpath.first, toplevel ? CRef::Toplevel : @lenv.cref, false)
          const_reads = [const_read]
          cpath[1..].each do |cname|
            const_read = ScopedConstRead.new(cname, const_read, false)
            const_reads << const_read
          end
          mod = genv.resolve_cpath(@cpath)
          const_read.followers << mod
          self_types << const_reads
        end
        static_ret
      end

      def undefine0(genv)
        super(genv)
        if @static_ret
          @static_ret[:self_types].each do |const_reads|
            const_reads.each do |const_read|
              const_read.destroy(genv)
            end
          end
        end
        @self_type_args.each do |args|
          args.each {|arg| arg.undefine(genv) }
        end
      end
    end

    class SigInterfaceNode < SigModuleBaseNode
    end

    class SigClassNode < SigModuleBaseNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        superclass = raw_decl.super_class
        if superclass
          name = superclass.name
          @superclass_cpath = name.namespace.path + [name.name]
          @superclass_toplevel = name.namespace.absolute?
          @superclass_args = superclass.args.map {|arg| AST.create_rbs_type(arg, lenv) }
        else
          @superclass_cpath = nil
          @superclass_toplevel = nil
          @superclass_args = nil
        end
      end

      attr_reader :superclass_cpath, :superclass_toplevel, :superclass_args

      def subnodes
        super.merge!({ superclass_args: })
      end
      def attrs
        super.merge!({ superclass_cpath:, superclass_toplevel: })
      end

      def define0(genv)
        static_ret = super(genv)
        const_reads = []
        if @superclass_cpath
          @superclass_args.each {|arg| arg.define(genv) }
          const_read = BaseConstRead.new(genv, @superclass_cpath.first, @superclass_toplevel ? CRef::Toplevel : @lenv.cref, false)
          const_reads << const_read
          @superclass_cpath[1..].each do |cname|
            const_read = ScopedConstRead.new(cname, const_read, false)
            const_reads << const_read
          end
          mod = genv.resolve_cpath(@cpath)
          const_read.followers << mod
        end
        static_ret[:superclass_cpath] = const_reads
        static_ret
      end

      def undefine0(genv)
        super(genv)
        if @static_ret
          @static_ret[:superclass_cpath].each do |const_read|
            const_read.destroy(genv)
          end
        end
        if @superclass_args
          @superclass_args.each {|arg| arg.undefine(genv) }
        end
      end
    end

    class SigDefNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @mid = raw_decl.name
        @mid_code_range = TypeProf::CodeRange.from_node(raw_decl.location[:name])
        @singleton = raw_decl.singleton?
        @instance = raw_decl.instance?
        @method_types = raw_decl.overloads.map do |overload|
          method_type = overload.method_type
          AST.create_rbs_func_type(method_type, method_type.type_params, method_type.block, lenv)
        end
        @overloading = raw_decl.overloading
      end

      attr_reader :mid, :singleton, :instance, :method_types, :overloading, :mid_code_range

      def subnodes = { method_types: }
      def attrs = { mid:, mid_code_range:, singleton:, instance:, overloading: }

      def mname_code_range(_name) = @mid_code_range

      def install0(genv)
        [[@singleton, true], [@instance, false]].each do |enabled, singleton|
          next unless enabled
          @changes.add_method_decl_box(genv, @lenv.cref.cpath, singleton, @mid, @method_types, @overloading)
        end
        Source.new
      end
    end

    class SigIncludeNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        name = raw_decl.name
        @cpath = name.namespace.path + [name.name]
        @toplevel = name.namespace.absolute?
        @args = raw_decl.args.map {|arg| AST.create_rbs_type(arg, lenv) }
      end

      attr_reader :cpath, :toplevel, :args
      def subnodes = { args: }
      def attrs = { cpath:, toplevel: }

      def define0(genv)
        @args.each {|arg| arg.define(genv) }
        const_reads = []
        const_read = BaseConstRead.new(genv, @cpath.first, @toplevel ? CRef::Toplevel : @lenv.cref, false)
        const_reads << const_read
        @cpath[1..].each do |cname|
          const_read = ScopedConstRead.new(cname, const_read, false)
          const_reads << const_read
        end
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        const_read.followers << mod
        mod.add_include_decl(genv, self)
        const_reads
      end

      def define_copy(genv)
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        mod.add_include_decl(genv, self)
        mod.remove_include_decl(genv, @prev_node)
        super(genv)
      end

      def undefine0(genv)
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        mod.remove_include_decl(genv, self)
        @static_ret.each do |const_read|
          const_read.destroy(genv)
        end
        @args.each {|arg| arg.undefine(genv) }
      end

      def install0(genv)
        Source.new
      end
    end

    class SigPrependNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        name = raw_decl.name
        @cpath = name.namespace.path + [name.name]
        @toplevel = name.namespace.absolute?
        @args = raw_decl.args.map {|arg| AST.create_rbs_type(arg, lenv) }
      end

      attr_reader :cpath, :toplevel, :args
      def subnodes = { args: }
      def attrs = { cpath:, toplevel: }

      def define0(genv)
        @args.each {|arg| arg.define(genv) }
        const_reads = []
        const_read = BaseConstRead.new(genv, @cpath.first, @toplevel ? CRef::Toplevel : @lenv.cref, false)
        const_reads << const_read
        @cpath[1..].each do |cname|
          const_read = ScopedConstRead.new(cname, const_read, false)
          const_reads << const_read
        end
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        const_read.followers << mod
        mod.add_prepend_decl(genv, self)
        const_reads
      end

      def define_copy(genv)
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        mod.add_prepend_decl(genv, self)
        mod.remove_prepend_decl(genv, @prev_node)
        super(genv)
      end

      def undefine0(genv)
        mod = genv.resolve_cpath(@lenv.cref.cpath)
        mod.remove_prepend_decl(genv, self)
        @static_ret.each do |const_read|
          const_read.destroy(genv)
        end
        @args.each {|arg| arg.undefine(genv) }
      end

      def install0(genv)
        Source.new
      end
    end

    class SigAliasNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @new_mid = raw_decl.new_name
        @old_mid = raw_decl.old_name
        @singleton = raw_decl.singleton?
        @instance = raw_decl.instance?
      end

      attr_reader :new_mid, :old_mid, :singleton, :instance
      def attrs = { new_mid:, old_mid:, singleton:, instance: }

      def install0(genv)
        [[@singleton, true], [@instance, false]].each do |enabled, singleton|
          next unless enabled
          @changes.add_method_alias_box(genv, @lenv.cref.cpath, singleton, @new_mid, @old_mid)
        end
        Source.new
      end
    end

    class SigAttrReaderNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @mid = raw_decl.name
        rbs_method_type = RBS::MethodType.new(
          type: RBS::Types::Function.empty(raw_decl.type),
          type_params: [],
          block: nil,
          location: raw_decl.type.location
        )
        @method_type = AST.create_rbs_func_type(rbs_method_type, [], nil, lenv)
      end

      attr_reader :mid, :method_type

      def subnodes = { method_type: }
      def attrs = { mid: }

      def install0(genv)
        @changes.add_method_decl_box(genv, @lenv.cref.cpath, false, @mid, [@method_type], false)
        Source.new
      end
    end

    class SigAttrWriterNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @mid = :"#{raw_decl.name}="

        # (raw_decl.type) -> raw_decl.type
        rbs_method_type = RBS::MethodType.new(
          type: RBS::Types::Function.new(
            required_positionals: [RBS::Types::Function::Param.new(name: nil, type: raw_decl.type, location: raw_decl.type.location)],
            optional_positionals: [],
            rest_positionals: nil,
            trailing_positionals: [],
            required_keywords: {},
            optional_keywords: {},
            rest_keywords: nil,
            return_type: raw_decl.type
          ),
          type_params: [],
          block: nil,
          location: raw_decl.type.location
        )
        @method_type = AST.create_rbs_func_type(rbs_method_type, [], nil, lenv)
      end

      attr_reader :mid, :method_type

      def subnodes = { method_type: }
      def attrs = { mid: }

      def install0(genv)
        @changes.add_method_decl_box(genv, @lenv.cref.cpath, false, @mid, [@method_type], false)
        Source.new
      end
    end

    class SigAttrAccessorNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @reader = SigAttrReaderNode.new(raw_decl, lenv)
        @writer = SigAttrWriterNode.new(raw_decl, lenv)
      end

      attr_reader :reader, :writer

      def subnodes = { reader:, writer: }

      def install0(genv)
        @reader.install0(genv)
        @writer.install0(genv)
        Source.new
      end
    end

    class SigConstNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @cpath = AST.resolve_rbs_name(raw_decl.name, lenv)
        @type = AST.create_rbs_type(raw_decl.type, lenv)
      end

      attr_reader :cpath, :type
      def subnodes = { type: }
      def attrs = { cpath: }

      def define0(genv)
        @type.define(genv)
        mod = genv.resolve_const(@cpath)
        mod.add_decl(self)
        mod
      end

      def define_copy(genv)
        mod = genv.resolve_const(@cpath)
        mod.add_decl(self)
        mod.remove_decl(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        genv.resolve_const(@cpath).remove_decl(self)
        @type.undefine(genv)
      end

      def install0(genv)
        box = @changes.add_type_read_box(genv, @type)
        @changes.add_edge(genv, box.ret, @static_ret.vtx)
        box.ret
      end
    end

    class SigTypeAliasNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @cpath = AST.resolve_rbs_name(raw_decl.name, lenv)
        @name = @cpath.pop
        @type = AST.create_rbs_type(raw_decl.type, lenv)
        @params = raw_decl.type_params.map {|param| param.name }
      end

      attr_reader :cpath, :name, :type, :params

      def define0(genv)
        @type.define(genv)
        tae = genv.resolve_type_alias(@cpath, @name)
        tae.add_decl(self)
        tae
      end

      def define_copy(genv)
        tae = genv.resolve_type_alias(@cpath, @name)
        tae.add_decl(self)
        tae.remove_decl(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        tae = genv.resolve_type_alias(@cpath, @name)
        tae.remove_decl(self)
        @type.undefine(genv)
      end

      def install0(genv)
        Source.new
      end
    end

    class SigInstanceVariableNode < Node
      def initialize(raw_decl, lenv, class_scope)
        super(raw_decl, lenv)
        @var = raw_decl.name
        @cpath = lenv.cref.cpath
        @class_scope = class_scope
        @type = AST.create_rbs_type(raw_decl.type, lenv)
      end

      attr_reader :cpath, :class_scope, :type
      def subnodes = { type: }
      def attrs = { cpath:, class_scope: }

      def define0(genv)
        @type.define(genv)
        mod = genv.resolve_ivar(cpath, @class_scope, @var)
        mod.add_decl(self)
        mod
      end

      def define_copy(genv)
        mod = genv.resolve_ivar(cpath, @class_scope, @var)
        mod.add_decl(self)
        mod.remove_decl(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        genv.resolve_ivar(cpath, @class_scope, @var).remove_decl(self)
        @type.undefine(genv)
      end

      def install0(genv)
        box = @changes.add_type_read_box(genv, @type)
        @changes.add_edge(genv, box.ret, @static_ret.vtx)
        box.ret
      end
    end

    class SigClassVariableNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @var = raw_decl.name
        @cpath = lenv.cref.cpath
        @type = AST.create_rbs_type(raw_decl.type, lenv)
      end

      attr_reader :cpath, :type
      def subnodes = { type: }
      def attrs = { cpath: }

      def define0(genv)
        @type.define(genv)
        cvar = genv.resolve_cvar(cpath, @var)
        cvar.add_decl(self)
        cvar
      end

      def define_copy(genv)
        cvar = genv.resolve_cvar(cpath, @var)
        cvar.add_decl(self)
        cvar.remove_decl(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        genv.resolve_cvar(cpath, @var).remove_decl(self)
        @type.undefine(genv)
      end

      def install0(genv)
        box = @changes.add_type_read_box(genv, @type)
        @changes.add_edge(genv, box.ret, @static_ret.vtx)
        box.ret
      end
    end

    class SigGlobalVariableNode < Node
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @var = raw_decl.name
        @type = AST.create_rbs_type(raw_decl.type, lenv)
      end

      attr_reader :cpath, :type
      def subnodes = { type: }
      def attrs = { cpath: }

      def define0(genv)
        @type.define(genv)
        mod = genv.resolve_gvar(@var)
        mod.add_decl(self)
        mod
      end

      def define_copy(genv)
        mod = genv.resolve_gvar(@var)
        mod.add_decl(self)
        mod.remove_decl(@prev_node)
        super(genv)
      end

      def undefine0(genv)
        genv.resolve_gvar(@var).remove_decl(self)
        @type.undefine(genv)
      end

      def install0(genv)
        box = @changes.add_type_read_box(genv, @type)
        @changes.add_edge(genv, box.ret, @static_ret.vtx)
        box.ret
      end
    end
  end
end
