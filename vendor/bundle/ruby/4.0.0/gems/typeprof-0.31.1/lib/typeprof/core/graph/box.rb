module TypeProf::Core
  $box_counts = Hash.new(0)
  class Box
    def initialize(node)
      @node = node
      @changes = ChangeSet.new(node, self)
      @destroyed = false
      $box_counts[Box] += 1
      $box_counts[self.class] += 1
    end

    attr_reader :changes

    attr_reader :node, :destroyed

    def destroy(genv)
      $box_counts[self.class] -= 1
      $box_counts[Box] -= 1
      @destroyed = true
      @changes.reinstall(genv) # rollback all changes
    end

    def reuse(new_node)
      @node = new_node
      @changes.reuse(new_node)
    end

    def on_type_added(genv, src_tyvar, added_types)
      genv.add_run(self)
    end

    def on_type_removed(genv, src_tyvar, removed_types)
      genv.add_run(self)
    end

    def run(genv)
      return if @destroyed
      run0(genv, @changes)
      @changes.reinstall(genv)
    end

    def run0(genv, changes)
      raise NotImplementedError
    end

    def to_s
      "#{ self.class.to_s.split("::").last }#{ @id ||= $new_id += 1 }"
    end

    alias inspect to_s
  end

  class ConstReadBox < Box
    def initialize(node, genv, const_read)
      super(node)
      @const_read = const_read
      const_read.followers << self
      @ret = Vertex.new(node)
      genv.add_run(self)
    end

    attr_reader :node, :const_read, :ret

    def run0(genv, changes)
      cdef = @const_read.cdef
      if cdef
        changes.add_depended_value_entity(cdef)
        changes.add_edge(genv, cdef.vtx, @ret)
      end
    end
  end

  class TypeReadBox < Box
    def initialize(node, genv, rbs_type)
      super(node)
      @rbs_type = rbs_type
      @ret = Vertex.new(node)
      genv.add_run(self)
    end

    attr_reader :node, :rbs_type, :ret

    def run0(genv, changes)
      # Create substitution map for type parameters if we're in a SigInstanceVariableNode within a generic class
      subst = {}
      if @node.is_a?(AST::SigInstanceVariableNode) && @node.cpath
        mod = genv.resolve_cpath(@node.cpath)
        if mod.type_params && !mod.type_params.empty?
          # Create a substitution map where each type parameter maps to a type variable vertex
          subst = mod.type_params.to_h do |param|
            type_var_vtx = Vertex.new(@node)
            [param, type_var_vtx]
          end
        end
      end

      vtx = @rbs_type.covariant_vertex(genv, changes, subst)
      changes.add_edge(genv, vtx, @ret)
    end
  end

  class MethodDeclBox < Box
    def initialize(node, genv, cpath, singleton, mid, method_types, overloading)
      super(node)
      @cpath = cpath
      @singleton = singleton
      @mid = mid
      @method_types = method_types
      @overloading = overloading
      @ret = Source.new

      me = genv.resolve_method(@cpath, @singleton, @mid)
      me.add_decl(self)
      me.add_run_all_method_call_boxes(genv)
      me.add_run_all_mdefs(genv)
    end

    attr_accessor :node

    attr_reader :cpath, :singleton, :mid, :method_types, :overloading, :ret

    def destroy(genv)
      me = genv.resolve_method(@cpath, @singleton, @mid)
      me.remove_decl(self)
      me.add_run_all_method_call_boxes(genv)
    end

    def match_arguments?(genv, changes, param_map, a_args, method_type)
      # TODO: handle a tuple as a splat argument?
      if a_args.splat_flags.any?
        return false unless method_type.rest_positionals
        method_type.req_positionals.size.times do |i|
          return false if a_args.splat_flags[i]
        end
        method_type.post_positionals.size.times do |i|
          return false if a_args.splat_flags[-i - 1]
        end
      else
        actual = a_args.positionals.size
        required_formal = method_type.req_positionals.size + method_type.post_positionals.size
        if actual < required_formal
          # too few actual arguments
          return false
        end
        if !method_type.rest_positionals && actual > required_formal + method_type.opt_positionals.size
          # too many actual arguments
          return false
        end
      end

      method_type.req_positionals.each_with_index do |ty, i|
        return false unless ty.typecheck(genv, changes, a_args.positionals[i], param_map)
      end
      method_type.post_positionals.each_with_index do |ty, i|
        i -= method_type.post_positionals.size
        return false unless ty.typecheck(genv, changes, a_args.positionals[i], param_map)
      end

      start_rest = method_type.req_positionals.size
      end_rest = a_args.positionals.size - method_type.post_positionals.size

      i = 0
      while i < method_type.opt_positionals.size && start_rest < end_rest
        break if a_args.splat_flags[start_rest]
        return false unless method_type.opt_positionals[i].typecheck(genv, changes, a_args.positionals[start_rest], param_map)
        i += 1
        start_rest += 1
      end

      if start_rest < end_rest
        vtxs = a_args.get_rest_args(genv, changes, start_rest, end_rest)
        while i < method_type.opt_positionals.size
          ty = method_type.opt_positionals[i]
          return false if vtxs.any? {|vtx| !ty.typecheck(genv, changes, vtx, param_map) }
          i += 1
        end
        if method_type.rest_positionals
          return false if vtxs.any? {|vtx| !method_type.rest_positionals.typecheck(genv, changes, vtx, param_map) }
        end
      end

      return true
    end

    def resolve_overload(changes, genv, method_type, node, param_map, a_args, ret, force)
      param_map0 = param_map.dup
      if method_type.type_params
        method_type.type_params.zip(yield(method_type)) do |var, vtx|
          param_map0[var] = vtx
        end
      end

      unless match_arguments?(genv, changes, param_map0, a_args, method_type)
        if force
          meth = node.mid_code_range ? :mid_code_range : :code_range
          changes.add_diagnostic(meth, "wrong type of arguments") # XXX: more friendly and fine-grained error message
        end
        return false
      end

      rbs_blk = method_type.block
      if method_type.block_required && !a_args.block
        if force
          meth = node.mid_code_range ? :mid_code_range : :code_range
          changes.add_diagnostic(meth, "block is expected") # XXX: more friendly error message
        end
        return false
      end
      if !rbs_blk && a_args.block
        if force
          meth = node.mid_code_range ? :mid_code_range : :code_range
          changes.add_diagnostic(meth, "block is not expected") # XXX: more friendly error message
        end
        return false
      end
      if rbs_blk && a_args.block
        # rbs_blk_func.optional_keywords, ...
        blk_a_args = rbs_blk.req_positionals.map do |blk_a_arg|
          blk_a_arg.covariant_vertex(genv, changes, param_map0)
        end
        a_args.block.each_type do |ty|
          case ty
          when Type::Proc
            ty.block.accept_args(genv, changes, blk_a_args)

            if ty.block.is_a?(Block)
              ty.block.next_boxes.each do |next_box|
                unless rbs_blk.return_type.typecheck(genv, changes, next_box.a_ret, param_map0)
                  next_box.wrong_return_type(rbs_blk.return_type.show, changes)
                end
              end
            end
          end
        end
      end

      force = true
      return true

    ensure
      if force
        ret_vtx = method_type.return_type.covariant_vertex(genv, changes, param_map0)
        changes.add_edge(genv, ret_vtx, ret)
      end
    end

    def resolve_overloads(changes, genv, node, param_map, a_args, ret, &blk)
      if @method_types.size == 1
        method_type = @method_types.first
        resolve_overload(changes, genv, method_type, node, param_map, a_args, ret, true, &blk)
        return
      end

      match_any_overload = false
      @method_types.each do |method_type|
        if resolve_overload(changes, genv, method_type, node, param_map, a_args, ret, false, &blk)
          match_any_overload = true
        end
      end
      unless match_any_overload
        meth = node.mid_code_range ? :mid_code_range : :code_range
        changes.add_diagnostic(meth, "failed to resolve overloads")
      end
    end

    def show
      @method_types.map do |method_type|
        args = []
        method_type.req_positionals.each do |arg|
          args << arg.show
        end
        method_type.opt_positionals.each do |arg|
          args << "?#{arg.show}"
        end
        if method_type.rest_positionals
          args << "*#{method_type.rest_positionals.show}"
        end
        method_type.post_positionals.each do |arg|
          args << arg.show
        end

        method_type.req_keywords.each do |key, arg|
          args << "#{ key }: #{arg.show}"
        end
        method_type.opt_keywords.each do |key, arg|
          args << "?#{ key }: #{arg.show}"
        end
        if method_type.rest_keywords
          args << "**#{method_type.rest_keywords.show}"
        end

        s = args.empty? ? "-> " : "(#{ args.join(", ") }) -> "
        s += method_type.return_type.show
      end.join(" | ")
    end
  end

  class EscapeBox < Box
    def initialize(node, genv, a_ret)
      super(node)
      @a_ret = a_ret.new_vertex(genv, node)
    end

    attr_reader :a_ret

    def ret = @a_ret

    def run0(genv, changes)
      return
    end

    def wrong_return_type(f_ret_show, changes)
      actual_ty = @a_ret.show
      return if actual_ty == "untyped" # XXX: too ad-hoc?
      msg = "expected: #{ f_ret_show }; actual: #{ actual_ty }"
      case @node
      when AST::ReturnNode
        changes.add_diagnostic(:code_range, msg, @node)
      when AST::DefNode
        changes.add_diagnostic(:last_stmt_code_range, msg, @node)
      when AST::NextNode
        changes.add_diagnostic(:code_range, msg, @node)
      when AST::CallNode
        changes.add_diagnostic(:block_last_stmt_code_range, msg, @node)
      when AST::AttrReaderMetaNode, AST::AttrAccessorMetaNode
        changes.add_diagnostic(:code_range, msg, @node)
      else
        pp @node.class
      end
    end
  end

  class SplatBox < Box
    def initialize(node, genv, ary, idx)
      super(node)
      @ary = ary
      @idx = idx
      @ary.add_edge(genv, self)
      @ret = Vertex.new(node)
    end

    attr_reader :ary, :idx, :ret

    def run0(genv, changes)
      @ary.each_type do |ty|
        case ty
        when Type::Instance
          if ty.mod == genv.mod_ary
            changes.add_edge(genv, ty.args[0], @ret)
          else
            "???"
          end
        when Type::Array
          if @idx && @idx < ty.elems.size
            changes.add_edge(genv, ty.elems[@idx], @ret)
          else
            changes.add_edge(genv, ty.get_elem(genv, @idx), @ret)
          end
        else
          "???"
        end
      end
    end
  end

  class HashSplatBox < Box
    def initialize(node, genv, hsh, unified_key, unified_val)
      super(node)
      @hsh = hsh
      @unified_key = unified_key
      @unified_val = unified_val
      @hsh.add_edge(genv, self)
    end

    def ret = @hsh # dummy

    attr_reader :hsh, :unified_key, :unified_val

    def run0(genv, changes)
      @hsh.each_type do |ty|
        ty = ty.base_type(genv)
        if ty.mod == genv.mod_hash
          changes.add_edge(genv, ty.args[0].new_vertex(genv, :__hash_splat), @unified_key)
          changes.add_edge(genv, ty.args[1].new_vertex(genv, :__hash_splat), @unified_val)
        else
          "???"
        end
      end
    end
  end

  class MethodDefBox < Box
    def initialize(node, genv, cpath, singleton, mid, f_args, ret_boxes)
      super(node)
      @cpath = cpath
      @singleton = singleton
      @mid = mid
      raise unless f_args
      @f_args = f_args
      raise unless f_args.is_a?(FormalArguments)

      @record_block = RecordBlock.new(@node)
      if @f_args.block
        record_blk_ty = Source.new(Type::Proc.new(genv, @record_block))
        record_blk_ty.add_edge(genv, @f_args.block)
      end

      @ret_boxes = ret_boxes
      @ret = Vertex.new(node)
      ret_boxes.each do |box|
        @changes.add_edge(genv, box.ret, @ret)
      end
      me = genv.resolve_method(@cpath, @singleton, @mid)
      me.add_def(self)
      me.add_run_all_method_call_boxes(genv) if me.decls.empty?
      genv.add_run(self)
    end

    attr_accessor :node

    attr_reader :cpath, :singleton, :mid, :f_args, :ret

    def destroy(genv)
      me = genv.resolve_method(@cpath, @singleton, @mid)
      me.remove_def(self)
      me.add_run_all_method_call_boxes(genv) if me.decls.empty?
      genv.add_run(self)
      super(genv)
    end

    def run0(genv, changes)
      me = genv.resolve_method(@cpath, @singleton, @mid)
      return if me.decls.empty?

      # TODO: support "| ..."
      decl = me.decls.to_a.first
      # TODO: support overload?
      method_type = decl.method_types.first
      _block = method_type.block

      mod = genv.resolve_cpath(@cpath)
      if @singleton
        ty = Type::Singleton.new(genv, mod)
        param_map0 = Type.default_param_map(genv, ty)
      else
        type_params = mod.type_params.map {|ty_param| Source.new() } # TODO: better support
        ty = Type::Instance.new(genv, mod, type_params)
        param_map0 = Type.default_param_map(genv, ty)
        if ty.is_a?(Type::Instance)
          ty.mod.type_params.zip(ty.args) do |param, arg|
            param_map0[param] = arg
          end
        end
      end
      method_type.type_params.each do |param|
        param_map0[param] = Source.new()
      end

      positional_args = []
      splat_flags = []

      method_type.req_positionals.each do |a_arg|
        positional_args << a_arg.contravariant_vertex(genv, changes, param_map0)
        splat_flags << false
      end
      method_type.opt_positionals.each do |a_arg|
        positional_args << a_arg.contravariant_vertex(genv, changes, param_map0)
        splat_flags << false
      end
      if method_type.rest_positionals
        elems = method_type.rest_positionals.contravariant_vertex(genv, changes, param_map0)
        positional_args << Source.new(genv.gen_ary_type(elems))
        splat_flags << true
      end
      method_type.post_positionals.each do |a_arg|
        positional_args << a_arg.contravariant_vertex(genv, changes, param_map0)
        splat_flags << false
      end

      a_args = ActualArguments.new(positional_args, splat_flags, nil, nil) # TODO: keywords and block
      if pass_arguments(changes, genv, a_args)
        # TODO: block
        f_ret = method_type.return_type.contravariant_vertex(genv, changes, param_map0)
        changes.add_edge(genv, f_ret, @ret)
        @ret_boxes.each do |ret_box|
          unless method_type.return_type.typecheck(genv, changes, ret_box.a_ret, param_map0)
            ret_box.wrong_return_type(method_type.return_type.show, changes)
          end
        end
      end
    end

    def pass_arguments(changes, genv, a_args)
      if a_args.splat_flags.any?
        # there is at least one splat actual argument

        lower = @f_args.req_positionals.size + @f_args.post_positionals.size
        upper = @f_args.rest_positionals ? nil : lower + @f_args.opt_positionals.size
        if upper && upper < a_args.positionals.size
          meth = changes.node.mid_code_range ? :mid_code_range : :code_range
          err = "#{ a_args.positionals.size } for #{ lower }#{ upper ? lower < upper ? "...#{ upper }" : "" : "+" }"
          changes.add_diagnostic(meth, "wrong number of arguments (#{ err })")
          return false
        end

        start_rest = [a_args.splat_flags.index(true), @f_args.req_positionals.size + @f_args.opt_positionals.size].min
        end_rest = [a_args.splat_flags.rindex(true) + 1, a_args.positionals.size - @f_args.post_positionals.size].max
        rest_vtxs = a_args.get_rest_args(genv, changes, start_rest, end_rest)

        @f_args.req_positionals.each_with_index do |f_vtx, i|
          if i < start_rest
            changes.add_edge(genv, a_args.positionals[i], f_vtx)
          else
            rest_vtxs.each do |vtx|
              changes.add_edge(genv, vtx, f_vtx)
            end
          end
        end
        @f_args.opt_positionals.each_with_index do |f_vtx, i|
          i += @f_args.opt_positionals.size
          if i < start_rest
            changes.add_edge(genv, a_args.positionals[i], f_vtx)
          else
            rest_vtxs.each do |vtx|
              changes.add_edge(genv, vtx, f_vtx)
            end
          end
        end
        @f_args.post_positionals.each_with_index do |f_vtx, i|
          i += a_args.positionals.size - @f_args.post_positionals.size
          if end_rest <= i
            changes.add_edge(genv, a_args.positionals[i], f_vtx)
          else
            rest_vtxs.each do |vtx|
              changes.add_edge(genv, vtx, f_vtx)
            end
          end
        end

        if @f_args.rest_positionals
          rest_vtxs.each do |vtx|
            @f_args.rest_positionals.each_type do |ty|
              if ty.is_a?(Type::Instance) && ty.mod == genv.mod_ary && ty.args[0]
                changes.add_edge(genv, vtx, ty.args[0])
              end
            end
          end
        end
      else
        # there is no splat actual argument

        lower = @f_args.req_positionals.size + @f_args.post_positionals.size
        upper = @f_args.rest_positionals ? nil : lower + @f_args.opt_positionals.size
        if a_args.positionals.size < lower || (upper && upper < a_args.positionals.size)
          meth = changes.node.mid_code_range ? :mid_code_range : :code_range
          err = "#{ a_args.positionals.size } for #{ lower }#{ upper ? lower < upper ? "...#{ upper }" : "" : "+" }"
          changes.add_diagnostic(meth, "wrong number of arguments (#{ err })")
          return false
        end

        @f_args.req_positionals.each_with_index do |f_vtx, i|
          changes.add_edge(genv, a_args.positionals[i], f_vtx)
        end
        @f_args.post_positionals.each_with_index do |f_vtx, i|
          i -= @f_args.post_positionals.size
          changes.add_edge(genv, a_args.positionals[i], f_vtx)
        end
        start_rest = @f_args.req_positionals.size
        end_rest = a_args.positionals.size - @f_args.post_positionals.size
        i = 0
        while i < @f_args.opt_positionals.size && start_rest < end_rest
          f_arg = @f_args.opt_positionals[i]
          changes.add_edge(genv, a_args.positionals[start_rest], f_arg)
          i += 1
          start_rest += 1
        end

        if start_rest < end_rest
          if @f_args.rest_positionals
            (start_rest..end_rest-1).each do |i|
              @f_args.rest_positionals.each_type do |ty|
                if ty.is_a?(Type::Instance) && ty.mod == genv.mod_ary && ty.args[0]
                  changes.add_edge(genv, a_args.positionals[i], ty.args[0])
                end
              end
            end
          end
        end
      end

      if a_args.keywords
        # TODO: support diagnostics
        @node.req_keywords.zip(@f_args.req_keywords) do |name, f_vtx|
          changes.add_edge(genv, a_args.get_keyword_arg(genv, changes, name), f_vtx)
        end

        @node.opt_keywords.zip(@f_args.opt_keywords).each do |name, f_vtx|
          changes.add_edge(genv, a_args.get_keyword_arg(genv, changes, name), f_vtx)
        end

        if @node.rest_keywords
          # FIXME: Extract the rest keywords excluding req_keywords and opt_keywords.
          changes.add_edge(genv, a_args.keywords, @f_args.rest_keywords)
        end
      end

      return true
    end

    def call(changes, genv, a_args, ret)
      if pass_arguments(changes, genv, a_args)
        changes.add_edge(genv, a_args.block, @f_args.block) if @f_args.block && a_args.block

        changes.add_edge(genv, @ret, ret)
      end
    end

    def show(output_parameter_names)
      block_show = []
      if @record_block.used
        blk_f_args = @record_block.f_args.map {|arg| arg.show }.join(", ")
        blk_ret = @record_block.ret.show
        block_show << "{ (#{ blk_f_args }) -> #{ blk_ret } }"
      end
      args = []
      @f_args.req_positionals.each do |f_vtx|
        args << Type.strip_parens(f_vtx.show)
      end
      @f_args.opt_positionals.each do |f_vtx|
        args << ("?" + Type.strip_parens(f_vtx.show))
      end
      if @f_args.rest_positionals
        args << ("*" + Type.strip_array(Type.strip_parens(@f_args.rest_positionals.show)))
      end
      @f_args.post_positionals.each do |var|
        args << Type.strip_parens(var.show)
      end
      if @node.is_a?(AST::DefNode)
        @node.req_keywords.zip(@f_args.req_keywords) do |name, f_vtx|
          args << "#{ name }: #{Type.strip_parens(f_vtx.show)}"
        end
        @node.opt_keywords.zip(@f_args.opt_keywords) do |name, f_vtx|
          args << "?#{ name }: #{Type.strip_parens(f_vtx.show)}"
        end
      end
      if @f_args.rest_keywords
        args << "**#{ Type.extract_hash_value_type(Type.strip_parens(@f_args.rest_keywords.show)) }"
      end

      if output_parameter_names && @node.is_a?(AST::DefNode)
        names = []
        names.concat(@node.req_positionals)
        names.concat(@node.opt_positionals)
        names.concat(@node.rest_positionals) if @node.rest_positionals
        names.concat(@node.post_positionals)
        names.concat(@node.req_keywords)
        names.concat(@node.opt_keywords)
        names.concat(@node.rest_keywords) if @node.rest_keywords
        args = args.zip(names).map do |arg, name|
          name ? "#{ arg } #{ name }" : arg
        end
      end

      args = args.join(", ")
      s = args.empty? ? [] : ["(#{ args })"]
      s << "#{ block_show.sort.join(" | ") }" unless block_show.empty?
      s << "-> #{ @mid == :initialize ? "void" : @ret.show }"
      s.join(" ")
    end
  end

  class MethodAliasBox < Box
    def initialize(node, genv, cpath, singleton, new_mid, old_mid)
      super(node)
      @cpath = cpath
      @singleton = singleton
      @new_mid = new_mid
      @old_mid = old_mid
      @ret = Source.new(genv.nil_type)

      me = genv.resolve_method(@cpath, @singleton, @new_mid)
      me.add_alias(self, @old_mid)
      me.add_run_all_method_call_boxes(genv) if me.decls.empty?
      genv.add_run(self)
    end

    attr_accessor :node

    attr_reader :cpath, :singleton, :new_mid, :old_mid, :ret

    def destroy(genv)
      me = genv.resolve_method(@cpath, @singleton, @new_mid)
      me.remove_alias(self)
      me.add_run_all_method_call_boxes(genv) if me.decls.empty?
      genv.add_run(self)
      super(genv)
    end

    def run0(genv, changes)
      # TODO: what to do?
    end
  end

  class MethodCallBox < Box
    def initialize(node, genv, recv, mid, a_args, subclasses)
      raise mid.to_s unless mid
      super(node)
      @recv = recv.new_vertex(genv, node)
      @recv.add_edge(genv, self)
      @mid = mid
      @a_args = a_args.new_vertexes(genv, node)
      @a_args.keywords.add_edge(genv, self) if @a_args.keywords
      @a_args.block.add_edge(genv, self) if @a_args.block
      @ret = Vertex.new(node)
      @subclasses = subclasses
      @generics = {}
    end

    attr_reader :recv, :mid, :ret

    def run0(genv, changes)
      edges = Set[]
      called_mdefs = Set[]
      error_count = 0
      resolve(genv, changes) do |me, ty, mid, orig_ty|
        if !me
          # TODO: undefined method error
          if error_count < 3
            meth = @node.mid_code_range ? :mid_code_range : :code_range
            changes.add_diagnostic(meth, "undefined method: #{ orig_ty.show }##{ mid }")
          end
          error_count += 1
        elsif me.builtin && me.builtin[changes, @node, orig_ty, @a_args, @ret]
          # do nothing
        elsif !me.decls.empty?
          # TODO: support "| ..."
          me.decls.each do |mdecl|
            # TODO: union type is ok?
            # TODO: add_depended_method_entity for types used to resolve overloads
            ty_env = Type.default_param_map(genv, orig_ty)
            if ty.is_a?(Type::Instance)
              ty.mod.type_params.zip(ty.args) do |param, arg|
                ty_env[param] = arg
              end
            end
            mdecl.resolve_overloads(changes, genv, @node, ty_env, @a_args, @ret) do |method_type|
              @generics[method_type] ||= method_type.type_params.map {|var| Vertex.new(@node) }
            end
          end
        elsif !me.defs.empty?
          me.defs.each do |mdef|
            next if called_mdefs.include?(mdef)
            called_mdefs << mdef
            mdef.call(changes, genv, @a_args, @ret)
          end
        else
          pp me
          raise
        end
      end
      if @subclasses
        resolve_subclasses(genv, changes) do |recv_ty, me|
          if !me.defs.empty?
            me.defs.each do |mdef|
              next if called_mdefs.include?(mdef)
              called_mdefs << mdef
              mdef.call(changes, genv, @a_args, @ret)
            end
          end
        end
      end
      edges.each do |src, dst|
        changes.add_edge(genv, src, dst)
      end
      if error_count > 3
        meth = @node.mid_code_range ? :mid_code_range : :code_range
        changes.add_diagnostic(meth, "... and other #{ error_count - 3 } errors")
      end
    end

    def resolve(genv, changes, &blk)
      @recv.each_type do |orig_ty|
        next if orig_ty == Type::Bot.new(genv)
        if @mid == :"*super"
          mid = @node.lenv.cref.mid
          skip = true
        else
          mid = @mid
          skip = false
        end

        ty = orig_ty.base_type(genv)

        base_ty_env = Type.default_param_map(genv, ty)

        alias_limit = 0
        while ty
          unless skip
            # First check prepended modules
            if !ty.is_a?(Type::Singleton)
              if resolve_prepended_modules(genv, changes, base_ty_env, ty, mid) do |me, ty, mid|
                  yield me, ty, mid, orig_ty
                end
                break
              end
            end

            # Then check the class/module itself
            me = ty.mod.get_method(ty.is_a?(Type::Singleton), mid)
            changes.add_depended_method_entity(me) if changes
            if !me.aliases.empty?
              mid = me.aliases.values.first
              alias_limit += 1
              redo if alias_limit < 5
            end
            if me.exist?
              yield me, ty, mid, orig_ty
              break
            end
          end

          skip = false

          if ty.is_a?(Type::Singleton)
            # TODO: extended modules
          else
            # Finally check included modules
            break if resolve_included_modules(genv, changes, base_ty_env, ty, mid) do |me, ty, mid|
              yield me, ty, mid, orig_ty
            end
          end

          ty = genv.get_superclass_type(ty, changes, base_ty_env)
        end

        yield nil, nil, mid, orig_ty unless ty
      end
    end

    def resolve_prepended_modules(genv, changes, base_ty_env, ty, mid, &blk)
      found = false

      alias_limit = 0
      # Process prepended modules in reverse order (last prepended = first in ancestor chain)
      ty.mod.prepended_modules.reverse_each do |prep_decl, prep_mod|
        if prep_decl.is_a?(AST::SigPrependNode) && prep_mod.type_params
          prep_ty = genv.get_instance_type(prep_mod, prep_decl.args, changes, base_ty_env, ty)
        else
          type_params = prep_mod.type_params.map {|ty_param| Source.new() } # TODO: better support
          prep_ty = Type::Instance.new(genv, prep_mod, type_params)
        end

        me = prep_ty.mod.get_method(false, mid)
        changes.add_depended_method_entity(me) if changes
        if !me.aliases.empty?
          mid = me.aliases.values.first
          alias_limit += 1
          redo if alias_limit < 5
        end
        if me.exist?
          found = true
          yield me, prep_ty, mid
        else
          found = resolve_prepended_modules(genv, changes, base_ty_env, prep_ty, mid, &blk)
        end
        break if found
      end

      found
    end

    def resolve_included_modules(genv, changes, base_ty_env, ty, mid, &blk)
      found = false

      alias_limit = 0
      ty.mod.self_types.each do |(mdecl, idx), self_ty_mod|
        raise unless mdecl.is_a?(AST::SigModuleNode)
        if self_ty_mod.type_params
          self_ty = genv.get_instance_type(self_ty_mod, mdecl.self_type_args[idx], changes, base_ty_env, ty)
        else
          self_ty = Type::Instance.new(genv, self_ty_mod, [])
        end

        me = self_ty.mod.get_method(false, mid)
        changes.add_depended_method_entity(me) if changes
        if !me.aliases.empty?
          mid = me.aliases.values.first
          alias_limit += 1
          redo if alias_limit < 5
        end
        if me.exist?
          found = true
          yield me, self_ty, mid
        else
          found ||= resolve_included_modules(genv, changes, base_ty_env, self_ty, mid, &blk)
        end
      end

      alias_limit = 0
      ty.mod.included_modules.each do |inc_decl, inc_mod|
        if inc_decl.is_a?(AST::SigIncludeNode) && inc_mod.type_params
          inc_ty = genv.get_instance_type(inc_mod, inc_decl.args, changes, base_ty_env, ty)
        else
          type_params = inc_mod.type_params.map {|ty_param| Source.new() } # TODO: better support
          inc_ty = Type::Instance.new(genv, inc_mod, type_params)
        end

        me = inc_ty.mod.get_method(false, mid)
        changes.add_depended_method_entity(me) if changes
        if !me.aliases.empty?
          mid = me.aliases.values.first
          alias_limit += 1
          redo if alias_limit < 5
        end
        if me.exist?
          found = true
          yield me, inc_ty, mid
        else
          found ||= resolve_included_modules(genv, changes, base_ty_env, inc_ty, mid, &blk)
        end
      end
      found
    end

    def resolve_subclasses(genv, changes)
      # TODO: This does not follow new subclasses
      @recv.each_type do |ty|
        next if ty == Type::Bot.new(genv)
        base_ty = ty.base_type(genv)
        singleton = base_ty.is_a?(Type::Singleton)
        mod = base_ty.mod
        mod.each_descendant do |desc_mod|
          next if mod == desc_mod
          me = desc_mod.get_method(singleton, @mid)
          changes.add_depended_method_entity(me)
          if me && me.exist?
            yield ty, me
          end
        end
      end
    end
  end

  class GVarReadBox < Box
    def initialize(node, genv, name)
      super(node)
      @vtx = genv.resolve_gvar(name).vtx
      @ret = Vertex.new(node)
      genv.add_run(self)
    end

    attr_reader :node, :const_read, :ret

    def run0(genv, changes)
      changes.add_edge(genv, @vtx, @ret)
    end
  end

  class IVarReadBox < Box
    def initialize(node, genv, cpath, singleton, name)
      super(node)
      @cpath = cpath
      @singleton = singleton
      @name = name
      genv.resolve_cpath(cpath).ivar_reads << self
      @proxy = Vertex.new(node)
      @ret = Vertex.new(node)
      genv.add_run(self)
    end

    attr_reader :node, :const_read, :ret

    def destroy(genv)
      genv.resolve_cpath(@cpath).ivar_reads.delete(self)
      super(genv)
    end

    def run0(genv, changes)
      mod = genv.resolve_cpath(@cpath)
      singleton = @singleton
      cur_ive = mod.get_ivar(singleton, @name)
      target_vtx = nil
      genv.each_direct_superclass(mod, singleton) do |mod, singleton|
        ive = mod.get_ivar(singleton, @name)
        if ive.exist?
          target_vtx = ive.vtx
        end
      end
      edges = []
      if target_vtx
        if target_vtx != cur_ive.vtx
          edges << [cur_ive.vtx, @proxy] << [@proxy, target_vtx]
        end
        edges << [target_vtx, @ret]
      else
        # TODO: error?
      end
      edges.each do |src, dst|
        changes.add_edge(genv, src, dst)
      end
    end
  end

  class CVarReadBox < Box
    def initialize(node, genv, cpath, name)
      super(node)
      @cpath = cpath
      @name = name
      genv.resolve_cpath(cpath).cvar_reads << self
      @proxy = Vertex.new(node)
      @ret = Vertex.new(node)
      genv.add_run(self)
    end

    attr_reader :node, :const_read, :ret

    def destroy(genv)
      genv.resolve_cpath(@cpath).cvar_reads.delete(self)
      super(genv)
    end

    def run0(genv, changes)
      mod = genv.resolve_cpath(@cpath)
      cur_cve = mod.get_cvar(@name)
      target_vtx = nil
      genv.each_direct_superclass(mod, nil) do |mod, _|
        cve = mod.get_cvar(@name)
        if cve.exist?
          target_vtx = cve.vtx
        end
      end

      edges = []
      if target_vtx
        if target_vtx != cur_cve.vtx
          edges << [cur_cve.vtx, @proxy] << [@proxy, target_vtx]
        end
        edges << [target_vtx, @ret]
      end

      edges.each do |src, dst|
        changes.add_edge(genv, src, dst)
      end
    end
  end

  class MAsgnBox < Box
    def initialize(node, genv, value, lefts, rest_elem, rights)
      super(node)
      @value = value
      @lefts = lefts
      @rest_elem = rest_elem
      @rights = rights
      @value.add_edge(genv, self)
    end

    attr_reader :node, :value, :lefts, :rest_elem, :rights

    def destroy(genv)
      @value.remove_edge(genv, self) # TODO: Is this really needed?
      super(genv)
    end

    def ret = @rhs

    def run0(genv, changes)
      edges = []
      @value.each_type do |ty|
        # TODO: call to_ary?
        case ty
        when Type::Array
          edges.concat(ty.splat_assign(genv, @lefts, @rest_elem, @rights))
        else
          if @lefts.size >= 1
            edges << [Source.new(ty), @lefts[0]]
          elsif @rights && @rights.size >= 1
            edges << [Source.new(ty), @rights[0]]
          else
            edges << [Source.new(ty), @rest_elem]
          end
        end
      end
      edges.each do |src, dst|
        changes.add_edge(genv, src, dst)
      end
    end
  end

  class InstanceTypeBox < Box
    def initialize(node, genv, singleton_ty_vtx)
      super(node)
      @singleton_ty_vtx = singleton_ty_vtx
      @ret = Vertex.new(node)
      genv.add_run(self)
    end

    attr_reader :ret

    def run0(genv, changes)
      instance_tys = []
      @singleton_ty_vtx.each_type do |ty|
        instance_tys << ty.get_instance_type(genv) if ty.is_a?(Type::Singleton)
      end
      source_vtx = Source.new(*instance_tys)
      changes.add_edge(genv, source_vtx, @ret)
    end
  end
end
