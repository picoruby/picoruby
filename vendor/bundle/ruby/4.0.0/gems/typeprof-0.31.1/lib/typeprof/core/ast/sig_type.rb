module TypeProf::Core
  class AST
    def self.typecheck_for_module(genv, changes, f_mod, f_args, a_vtx, subst)
      changes.add_edge(genv, a_vtx, changes.target)
      a_vtx.each_type do |ty|
        ty = ty.base_type(genv)
        while ty
          if ty.mod == f_mod && ty.is_a?(Type::Instance)
            args_all_match = true
            f_args.zip(ty.args) do |f_arg_node, a_arg_ty|
              unless f_arg_node.typecheck(genv, changes, a_arg_ty, subst)
                args_all_match = false
                break
              end
            end
            return true if args_all_match
          end
          changes.add_depended_superclass(ty.mod)

          if f_mod.module?
            return true if typecheck_for_prepended_modules(genv, changes, ty, f_mod, f_args, subst)
            return true if typecheck_for_included_modules(genv, changes, ty, f_mod, f_args, subst)
          end

          ty = genv.get_superclass_type(ty, changes, {})
        end
      end
      return false
    end

    def self.typecheck_for_prepended_modules(genv, changes, a_ty, f_mod, f_args, subst)
      a_ty.mod.prepended_modules.each do |prep_decl, prep_mod|
        if prep_decl.is_a?(AST::SigPrependNode) && prep_mod.type_params
          prep_ty = genv.get_instance_type(prep_mod, prep_decl.args, changes, {}, a_ty)
        else
          type_params = prep_mod.type_params.map {|ty_param| Source.new() } # TODO: better support
          prep_ty = Type::Instance.new(genv, prep_mod, type_params)
        end
        if prep_ty.mod == f_mod
          args_all_match = true
          f_args.zip(prep_ty.args) do |f_arg_node, a_arg_ty|
            unless f_arg_node.typecheck(genv, changes, a_arg_ty, subst)
              args_all_match = false
              break
            end
          end
          return true if args_all_match
        end
        changes.add_depended_superclass(prep_ty.mod)

        return true if typecheck_for_prepended_modules(genv, changes, prep_ty, f_mod, f_args, subst)
      end
      return false
    end

    def self.typecheck_for_included_modules(genv, changes, a_ty, f_mod, f_args, subst)
      a_ty.mod.included_modules.each do |inc_decl, inc_mod|
        if inc_decl.is_a?(AST::SigIncludeNode) && inc_mod.type_params
          inc_ty = genv.get_instance_type(inc_mod, inc_decl.args, changes, {}, a_ty)
        else
          type_params = inc_mod.type_params.map {|ty_param| Source.new() } # TODO: better support
          inc_ty = Type::Instance.new(genv, inc_mod, type_params)
        end
        if inc_ty.mod == f_mod
          args_all_match = true
          f_args.zip(inc_ty.args) do |f_arg_node, a_arg_vtx|
            unless f_arg_node.typecheck(genv, changes, a_arg_vtx, subst)
              args_all_match = false
              break
            end
          end
          return true if args_all_match
        end
        changes.add_depended_superclass(inc_ty.mod)

        return true if typecheck_for_included_modules(genv, changes, inc_ty, f_mod, f_args, subst)
      end
      return false
    end

    class SigFuncType < Node
      def initialize(raw_decl, raw_type_params, raw_block, lenv)
        super(raw_decl, lenv)
        if raw_block
          @block_required = raw_block.required
          @block = AST.create_rbs_func_type(raw_block, nil, nil, lenv)
        else
          @block_required = false
          @block = nil
        end

        # TODO?: param.variance, param.unchecked, param.upper_bound
        @type_params = raw_type_params ? raw_type_params.map {|param| param.name } : nil

        if raw_decl.type.is_a?(RBS::Types::Function)
          @req_positionals = raw_decl.type.required_positionals.map do |ty|
            raise "unsupported argument type: #{ ty.class }" if !ty.is_a?(RBS::Types::Function::Param)
            AST.create_rbs_type(ty.type, lenv)
          end
          @post_positionals = raw_decl.type.trailing_positionals.map do |ty|
            raise "unsupported argument type: #{ ty.class }" if !ty.is_a?(RBS::Types::Function::Param)
            AST.create_rbs_type(ty.type, lenv)
          end
          @opt_positionals = raw_decl.type.optional_positionals.map do |ty|
            raise "unsupported argument type: #{ ty.class }" if !ty.is_a?(RBS::Types::Function::Param)
            AST.create_rbs_type(ty.type, lenv)
          end
          param = raw_decl.type.rest_positionals
          @rest_positionals = param ? AST.create_rbs_type(param.type, lenv) : nil

          @req_keywords = raw_decl.type.required_keywords.to_h do |key, ty|
            raise "unsupported argument type: #{ ty.class }" if !ty.is_a?(RBS::Types::Function::Param)
            [key, AST.create_rbs_type(ty.type, lenv)]
          end
          @opt_keywords = raw_decl.type.optional_keywords.to_h do |key, ty|
            raise "unsupported argument type: #{ ty.class }" if !ty.is_a?(RBS::Types::Function::Param)
            [key, AST.create_rbs_type(ty.type, lenv)]
          end
          param = raw_decl.type.rest_keywords
          @rest_keywords = param ? AST.create_rbs_type(param.type, lenv) : nil
        else
          # RBS::Types::UntypedFunction
          @req_positionals = []
          @post_positionals = []
          @opt_positionals = []
          @rest_positionals = SigTyBaseAnyNode.new(raw_decl, lenv)
          @req_keywords = {}
          @opt_keywords = {}
          @rest_keywords = nil
        end

        @return_type = AST.create_rbs_type(raw_decl.type.return_type, lenv)
      end

      attr_reader :type_params, :block, :block_required
      attr_reader :req_positionals
      attr_reader :post_positionals
      attr_reader :opt_positionals
      attr_reader :rest_positionals
      attr_reader :req_keywords
      attr_reader :opt_keywords
      attr_reader :rest_keywords
      attr_reader :return_type

      def subnodes = {
        block:,
        req_positionals:,
        post_positionals:,
        opt_positionals:,
        rest_positionals:,
        req_keywords:,
        opt_keywords:,
        rest_keywords:,
        return_type:,
      }
      def attrs = { type_params:, block_required: }
    end

    class SigTyNode < Node
      def covariant_vertex(genv, changes, subst)
        vtx = changes.new_covariant_vertex(genv, self)
        covariant_vertex0(genv, changes, vtx, subst)
        vtx
      end

      def contravariant_vertex(genv, changes, subst)
        vtx = changes.new_contravariant_vertex(genv, self)
        contravariant_vertex0(genv, changes, vtx, subst)
        vtx
      end
    end

    class SigTyBaseBoolNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.true_type, genv.false_type), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.true_type, genv.false_type), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_edge(genv, vtx, changes.target)
        vtx.each_type do |ty|
          return false unless ty == genv.true_type || ty == genv.false_type
        end
        true
      end

      def show
        "bool"
      end
    end

    class SigTyBaseNilNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.nil_type), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.nil_type), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_edge(genv, vtx, changes.target)
        vtx.each_type do |ty|
          return false unless ty == genv.nil_type
        end
        true
      end

      def show
        "nil"
      end
    end

    class SigTyBaseSelfNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, subst[:"*self"], vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, subst[:"*self"], vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        true # TODO: check self type
      end

      def show
        "self"
      end
    end

    class SigTyBaseVoidNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.obj_type), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.obj_type), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        true
      end

      def show
        "void"
      end
    end

    class SigTyBaseAnyNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        #Source.new(genv.obj_type).add_edge(genv, vtx) # TODO
      end

      def typecheck(genv, changes, vtx, subst)
        true
      end

      def show
        "untyped"
      end
    end

    class SigTyBaseTopNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        # TODO
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        # TODO
      end

      def typecheck(genv, changes, vtx, subst)
        true
      end

      def show
        "top"
      end
    end

    class SigTyBaseBottomNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(Type::Bot.new(genv)), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(Type::Bot.new(genv)), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_edge(genv, vtx, changes.target)
        vtx.types.empty?
      end

      def show
        "bot"
      end
    end

    class SigTyBaseInstanceNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, subst[:"*instance"], vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, subst[:"*instance"], vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        true # TODO: implement
      end

      def show
        "instance"
      end
    end

    class SigTyBaseClassNode < SigTyNode
      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, subst[:"*class"], vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, subst[:"*class"], vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        true # TODO: implement
      end

      def show
        "class"
      end
    end

    class SigTyAliasNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        name = raw_decl.name
        @cpath = name.namespace.path
        @toplevel = name.namespace.absolute?
        @name = name.name
        @args = raw_decl.args.map {|arg| AST.create_rbs_type(arg, lenv) }
      end

      attr_reader :cpath, :toplevel, :name, :args
      def subnodes = { args: }
      def attrs = { cpath:, toplevel:, name: }

      def define0(genv)
        @args.each {|arg| arg.define(genv) }

        static_reads = []
        if @cpath.empty?
          static_reads << BaseTypeAliasRead.new(genv, @name, @toplevel ? CRef::Toplevel : @lenv.cref, false)
        else
          static_reads << BaseConstRead.new(genv, @cpath.first, @toplevel ? CRef::Toplevel : @lenv.cref, false)
          @cpath[1..].each do |cname|
            static_reads << ScopedConstRead.new(cname, static_reads.last, false)
          end
          static_reads << ScopedTypeAliasRead.new(@name, static_reads.last, false)
        end
        static_reads
      end

      def undefine0(genv)
        @static_ret.each do |static_read|
          static_read.destroy(genv)
        end
        @args.each {|arg| arg.undefine(genv) }
      end

      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        tae = @static_ret.last.type_alias_entity
        if tae && tae.exist?
          # Check for recursive expansion
          expansion_key = [@cpath, @name]
          subst[:__expansion_stack__] ||= []

          if subst[:__expansion_stack__].include?(expansion_key)
            # Recursive expansion detected: this type alias references itself
            # Stop expansion here to prevent SystemStackError. The type system
            # will handle the incomplete expansion gracefully, typically by
            # treating unresolved recursive references as 'untyped', which
            # maintains type safety while allowing the program to continue.
            return
          end

          # need to check tae decls are all consistent?
          decl = tae.decls.each {|decl| break decl }
          subst0 = subst.dup
          subst0[:__expansion_stack__] = subst[:__expansion_stack__].dup + [expansion_key]

          # raise if decl.params.size != @args.size # ?
          decl.params.zip(@args) do |param, arg|
            subst0[param] = arg.covariant_vertex(genv, changes, subst0) # passing subst0 is ok?
          end
          tae.type.covariant_vertex0(genv, changes, vtx, subst0)
        end
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        tae = @static_ret.last.type_alias_entity
        if tae && tae.exist?
          # Check for recursive expansion
          expansion_key = [@cpath, @name]
          subst[:__expansion_stack__] ||= []

          if subst[:__expansion_stack__].include?(expansion_key)
            # Recursive expansion detected: this type alias references itself
            # Stop expansion here to prevent SystemStackError. The type system
            # will handle the incomplete expansion gracefully, typically by
            # treating unresolved recursive references as 'untyped', which
            # maintains type safety while allowing the program to continue.
            return
          end

          # need to check tae decls are all consistent?
          decl = tae.decls.each {|decl| break decl }
          subst0 = subst.dup
          subst0[:__expansion_stack__] = subst[:__expansion_stack__].dup + [expansion_key]

          # raise if decl.params.size != @args.size # ?
          decl.params.zip(@args) do |param, arg|
            subst0[param] = arg.contravariant_vertex(genv, changes, subst0)
          end
          tae.type.contravariant_vertex0(genv, changes, vtx, subst0)
        end
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        tae = @static_ret.last.type_alias_entity
        if tae && tae.exist?
          # TODO: check for recursive expansion
          decl = tae.decls.each {|decl| break decl }
          subst0 = subst.dup
          decl.params.zip(@args) do |param, arg|
            subst0[param] = arg.covariant_vertex(genv, changes, subst0)
          end
          tae.type.typecheck(genv, changes, vtx, subst0)
        end
      end

      def show
        "(...alias...)"
      end
    end

    class SigTyUnionNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @types = (raw_decl.types || []).map {|type| AST.create_rbs_type(type, lenv) }
      end

      attr_reader :types

      def subnodes = { types: }

      def covariant_vertex0(genv, changes, vtx, subst)
        @types.each do |type|
          type.covariant_vertex0(genv, changes, vtx, subst)
        end
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        @types.each do |type|
          type.contravariant_vertex0(genv, changes, vtx, subst)
        end
      end

      def typecheck(genv, changes, vtx, subst)
        @types.each do |type|
          return true if type.typecheck(genv, changes, vtx, subst)
        end
        false
      end

      def show
        @types.map {|ty| ty.show }.join(" | ")
      end
    end

    class SigTyIntersectionNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @types = (raw_decl.types || []).map {|type| AST.create_rbs_type(type, lenv) }
      end

      attr_reader :types

      def subnodes = { types: }

      def covariant_vertex0(genv, changes, vtx, subst)
        @types.each do |type|
          type.covariant_vertex0(genv, changes, vtx, subst)
        end
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        @types.each do |type|
          type.contravariant_vertex0(genv, changes, vtx, subst)
        end
      end

      def typecheck(genv, changes, vtx, subst)
        @types.each do |type|
          return false unless type.typecheck(genv, changes, vtx, subst)
        end
        true
      end

      def show
        @types.map {|ty| ty.show }.join(" & ")
      end
    end

    class SigTySingletonNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        name = raw_decl.name
        @cpath = name.namespace.path + [name.name]
        @toplevel = name.namespace.absolute?
      end

      attr_reader :cpath, :toplevel
      def attrs = { cpath:, toplevel: }

      def define0(genv)
        const_reads = []
        const_read = BaseConstRead.new(genv, @cpath.first, @toplevel ? CRef::Toplevel : @lenv.cref, false)
        const_reads << const_read
        unless @cpath.empty?
          @cpath[1..].each do |cname|
            const_read = ScopedConstRead.new(cname, const_read, false)
            const_reads << const_read
          end
        end
        const_reads
      end

      def undefine0(genv)
        @static_ret.each do |const_read|
          const_read.destroy(genv)
        end
      end

      def covariant_vertex0(genv, changes, vtx, subst)
        # TODO: type.args
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        mod = genv.resolve_cpath(cpath)
        changes.add_edge(genv, Source.new(Type::Singleton.new(genv, mod)), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        # TODO: type.args
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        mod = genv.resolve_cpath(cpath)
        changes.add_edge(genv, Source.new(Type::Singleton.new(genv, mod)), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        f_mod = genv.resolve_cpath(cpath)
        changes.add_edge(genv, vtx, changes.target)
        vtx.each_type do |ty|
          case ty
          when Type::Singleton
            if f_mod.module?
              # TODO: implement
            else
              a_mod = ty.mod
              while a_mod
                return true if a_mod == f_mod
                changes.add_depended_superclass(a_mod)
                a_mod = a_mod.superclass
              end
            end
          end
        end
        false
      end

      def show
        s = "::#{ @cpath.join("::") }"
        if !@args.empty?
          s << "[...]"
        end
        "singleton(#{ s })"
      end
    end

    class SigTyInstanceNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        name = raw_decl.name
        @cpath = name.namespace.path + [name.name]
        @toplevel = name.namespace.absolute? # "::Foo" or "Foo"
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
        unless @cpath.empty?
          @cpath[1..].each do |cname|
            const_read = ScopedConstRead.new(cname, const_read, false)
            const_reads << const_read
          end
        end
        const_reads
      end

      def undefine0(genv)
        @static_ret.each do |const_read|
          const_read.destroy(genv)
        end
        @args.each {|arg| arg.undefine(genv) }
      end

      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        mod = genv.resolve_cpath(cpath)
        args = @args.map {|arg| arg.covariant_vertex(genv, changes, subst) }
        changes.add_edge(genv, Source.new(Type::Instance.new(genv, mod, args)), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        mod = genv.resolve_cpath(cpath)
        # TODO: report error for wrong type arguments
        # TODO: support default type args
        args = mod.type_params.zip(@args).map do |_, arg|
          arg ? arg.contravariant_vertex(genv, changes, subst) : Source.new
        end
        changes.add_edge(genv, Source.new(Type::Instance.new(genv, mod, args)), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        f_mod = genv.resolve_cpath(cpath)
        AST.typecheck_for_module(genv, changes, f_mod, @args, vtx, subst)
      end

      def show
        cpath = @static_ret.last.cpath
        if cpath
          s = "#{ cpath.join("::") }"
          if !@args.empty?
            s << "[...]"
          end
          s
        else
          "(unknown instance)"
        end
      end
    end

    class SigTyTupleNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @types = raw_decl.types.map {|type| AST.create_rbs_type(type, lenv) }
      end

      attr_reader :types
      def subnodes = { types: }

      def covariant_vertex0(genv, changes, vtx, subst)
        unified_elem = changes.new_covariant_vertex(genv, [self, :Elem]) # TODO
        elems = @types.map do |type|
          nvtx = type.covariant_vertex(genv, changes, subst)
          changes.add_edge(genv, nvtx, unified_elem)
          nvtx
        end
        changes.add_edge(genv, Source.new(Type::Array.new(genv, elems, genv.gen_ary_type(unified_elem))), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        unified_elem = changes.new_contravariant_vertex(genv, [self, :Elem]) # TODO
        elems = @types.map do |type|
          nvtx = type.contravariant_vertex(genv, changes, subst)
          changes.add_edge(genv, nvtx, unified_elem)
          nvtx
        end
        changes.add_edge(genv, Source.new(Type::Array.new(genv, elems, genv.gen_ary_type(unified_elem))), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_edge(genv, vtx, changes.target)
        vtx.each_type do |ty|
          case ty
          when Type::Array
            next if ty.elems.size != @types.size
            @types.zip(ty.elems) do |f_ty, a_ty|
              return false unless f_ty.typecheck(genv, changes, a_ty, subst)
            end
            return true
          when Type::Instance
            @types.each do |f_ty|
              return false unless f_ty.typecheck(genv, changes, vtx, subst)
            end
            return true
          end
        end
        false
      end

      def show
        "[#{ @types.map {|ty| ty.show }.join(", ") }]"
      end
    end

    class SigTyRecordNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @fields = raw_decl.fields.transform_values { |val| AST.create_rbs_type(val, lenv) }
      end

      attr_reader :fields
      def subnodes = { fields: }

      def covariant_vertex0(genv, changes, vtx, subst)
        field_vertices = {}
        @fields.each do |key, field_node|
          field_vertices[key] = field_node.covariant_vertex(genv, changes, subst)
        end

        # Create base Hash type for Record
        key_vtx = Source.new(genv.symbol_type)
        # Create union of all field values for the Hash value type
        val_vtx = changes.new_covariant_vertex(genv, [self, :union])
        field_vertices.each_value do |field_vtx|
          changes.add_edge(genv, field_vtx, val_vtx)
        end
        base_hash_type = genv.gen_hash_type(key_vtx, val_vtx)

        changes.add_edge(genv, Source.new(Type::Record.new(genv, field_vertices, base_hash_type)), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        field_vertices = {}
        @fields.each do |key, field_node|
          field_vertices[key] = field_node.contravariant_vertex(genv, changes, subst)
        end

        # Create base Hash type for Record
        key_vtx = Source.new(genv.symbol_type)
        # Create union of all field values for the Hash value type
        val_vtx = changes.new_contravariant_vertex(genv, [self, :union])
        field_vertices.each_value do |field_vtx|
          changes.add_edge(genv, field_vtx, val_vtx)
        end
        base_hash_type = genv.gen_hash_type(key_vtx, val_vtx)

        changes.add_edge(genv, Source.new(Type::Record.new(genv, field_vertices, base_hash_type)), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_edge(genv, vtx, changes.target)
        vtx.each_type do |ty|
          case ty
          when Type::Hash
            @fields.each do |key, field_node|
              val_vtx = ty.get_value(key)
              return false unless field_node.typecheck(genv, changes, val_vtx, subst)
            end
            return true
          end
        end
        false
      end

      def show
        field_strs = @fields.map do |key, field_node|
          "#{ key }: #{ field_node.show }"
        end
        "{ #{ field_strs.join(", ") } }"
      end
    end

    class SigTyVarNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @var = raw_decl.name
      end

      attr_reader :var

      def attrs = { var: }

      def covariant_vertex0(genv, changes, vtx, subst)
        raise "unknown type variable: #{ @var }" unless subst[@var]
        changes.add_edge(genv, subst[@var], vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        raise "unknown type variable: #{ @var }" unless subst[@var]
        changes.add_edge(genv, Source.new(Type::Var.new(genv, @var, subst[@var])), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_edge(genv, vtx.new_vertex(genv, self), subst[@var]) unless vtx == subst[@var]
        true
      end

      def show
        "#{ @var }"
      end
    end

    class SigTyOptionalNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @type = AST.create_rbs_type(raw_decl.type, lenv)
      end

      attr_reader :type
      def subnodes = { type: }

      def covariant_vertex0(genv, changes, vtx, subst)
        @type.covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.nil_type), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        @type.contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(genv.nil_type), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        @type.typecheck(genv, changes, vtx, subst)
      end

      def show
        s = @type.show
        if @type.is_a?(SigTyIntersectionNode) || @type.is_a?(SigTyUnionNode)
          s = "(#{ s })"
        end
        s + "?"
      end
    end

    class SigTyLiteralNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        @lit = raw_decl.literal
      end

      attr_reader :lit
      def attrs = { lit: }

      def get_type(genv)
        case @lit
        when ::Symbol
          Type::Symbol.new(genv, @lit)
        when ::Integer then genv.int_type
        when ::String then genv.str_type
        when ::TrueClass then genv.true_type
        when ::FalseClass then genv.false_type
        else
          raise "unknown RBS literal: #{ @lit.inspect }"
        end
      end

      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(get_type(genv)), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_edge(genv, Source.new(get_type(genv)), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        if @lit.is_a?(::Symbol)
          changes.add_edge(genv, vtx, changes.target)
          vtx.each_type do |ty|
            case ty
            when Type::Symbol
              return true if ty.sym == @lit
            end
          end
          return false
        end
        f_mod = get_type(genv).mod
        AST.typecheck_for_module(genv, changes, f_mod, [], vtx, subst)
      end

      def show
        @lit.inspect
      end
    end

    class SigTyProcNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        # raw_decl.type is an RBS::Types::Function, we need to wrap it in a MethodType
        if raw_decl.type
          method_type = RBS::MethodType.new(
            type: raw_decl.type,
            type_params: [],
            block: raw_decl.block,
            location: raw_decl.location
          )
          @type = AST.create_rbs_func_type(method_type, nil, raw_decl.block, lenv)
        else
          @type = nil
        end
      end

      attr_reader :type
      def subnodes = { type: }

      def covariant_vertex0(genv, changes, vtx, subst)
        # For now, just return the base Proc type without the function signature details
        # TODO: Create a proper Type::Proc with the function signature
        changes.add_edge(genv, Source.new(genv.proc_type), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        # For now, just return the base Proc type without the function signature details
        # TODO: Create a proper Type::Proc with the function signature
        changes.add_edge(genv, Source.new(genv.proc_type), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        # TODO: proper check
        AST.typecheck_for_module(genv, changes, genv.proc_type.mod, [], vtx, subst)
      end

      def show
        "^(...)"
      end
    end

    class SigTyInterfaceNode < SigTyNode
      def initialize(raw_decl, lenv)
        super(raw_decl, lenv)
        name = raw_decl.name
        @cpath = name.namespace.path + [name.name]

        @toplevel = name.namespace.absolute? # "::Foo" or "Foo"
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
        unless @cpath.empty?
          @cpath[1..].each do |cname|
            const_read = ScopedConstRead.new(cname, const_read, false)
            const_reads << const_read
          end
        end
        const_reads
      end

      def undefine0(genv)
        @static_ret.each do |const_read|
          const_read.destroy(genv)
        end
        @args.each {|arg| arg.undefine(genv) }
      end

      def covariant_vertex0(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        mod = genv.resolve_cpath(cpath)
        args = @args.map {|arg| arg.covariant_vertex(genv, changes, subst) }
        changes.add_edge(genv, Source.new(Type::Instance.new(genv, mod, args)), vtx)
      end

      def contravariant_vertex0(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        mod = genv.resolve_cpath(cpath)
        args = @args.map {|arg| arg.contravariant_vertex(genv, changes, subst) }
        changes.add_edge(genv, Source.new(Type::Instance.new(genv, mod, args)), vtx)
      end

      def typecheck(genv, changes, vtx, subst)
        changes.add_depended_static_read(@static_ret.last)
        cpath = @static_ret.last.cpath
        return unless cpath
        f_mod = genv.resolve_cpath(cpath)
        # self/f_mod: formal, vtx: actual
        AST.typecheck_for_module(genv, changes, f_mod, @args, vtx, subst)
      end

      def show
        s = "::#{ @cpath.join("::") }"
        if !@args.empty?
          s << "[...]"
        end
        s
      end
    end
  end
end
