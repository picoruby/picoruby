module TypeProf::Core
  class GlobalEnv
    def initialize
      @type_table = {}

      @static_eval_queue = []

      @run_queue = []
      @run_queue_set = Set[]

      @pending_diagnostic_paths = Set[]

      @mod_object = ModuleEntity.new([])
      @mod_object.inner_modules[:Object] = @mod_object

      @mod_basic_object = resolve_cpath([:BasicObject])
      @mod_class = resolve_cpath([:Class])
      @mod_module = resolve_cpath([:Module])

      @gvars = {}
      @mod_ary = resolve_cpath([:Array])
      @mod_hash = resolve_cpath([:Hash])
      @mod_range = resolve_cpath([:Range])
      @mod_str = resolve_cpath([:String])

      @cls_type = Type::Instance.new(self, @mod_class, [])
      @mod_type = Type::Instance.new(self, @mod_module, [])
      @obj_type = Type::Instance.new(self, resolve_cpath([:Object]), [])
      @nil_type = Type::Instance.new(self, resolve_cpath([:NilClass]), [])
      @true_type = Type::Instance.new(self, resolve_cpath([:TrueClass]), [])
      @false_type = Type::Instance.new(self, resolve_cpath([:FalseClass]), [])
      @str_type = Type::Instance.new(self, resolve_cpath([:String]), [])
      @int_type = Type::Instance.new(self, resolve_cpath([:Integer]), [])
      @float_type = Type::Instance.new(self, resolve_cpath([:Float]), [])
      @rational_type = Type::Instance.new(self, resolve_cpath([:Rational]), [])
      @complex_type = Type::Instance.new(self, resolve_cpath([:Complex]), [])
      @proc_type = Type::Instance.new(self, resolve_cpath([:Proc]), [])
      @symbol_type = Type::Instance.new(self, resolve_cpath([:Symbol]), [])
      @set_type = Type::Instance.new(self, resolve_cpath([:Set]), [])
      @regexp_type = Type::Instance.new(self, resolve_cpath([:Regexp]), [])

      @run_count = 0
    end

    attr_reader :type_table

    attr_reader :mod_class, :mod_object, :mod_ary, :mod_hash, :mod_range, :mod_str
    attr_reader :cls_type, :mod_type
    attr_reader :obj_type, :nil_type, :true_type, :false_type, :str_type
    attr_reader :int_type, :float_type, :rational_type, :complex_type
    attr_reader :proc_type, :symbol_type, :set_type, :regexp_type

    def gen_ary_type(elem_vtx)
      Type::Instance.new(self, @mod_ary, [elem_vtx])
    end

    def gen_hash_type(key_vtx, val_vtx)
      Type::Instance.new(self, @mod_hash, [key_vtx, val_vtx])
    end

    def gen_range_type(elem_vtx)
      Type::Instance.new(self, @mod_range, [elem_vtx])
    end

    attr_accessor :run_count

    def each_direct_superclass(mod, singleton)
      while mod
        yield mod, singleton
        singleton, mod = get_superclass(singleton, mod)
      end
    end

    def each_superclass(mod, singleton, &blk)
      while mod
        # TODO: prepended modules
        yield mod, singleton
        if singleton
          # TODO: extended modules
        else
          each_included_module(mod, &blk)
        end
        singleton, mod = get_superclass(singleton, mod)
      end
    end

    def each_included_module(mod, &blk)
      mod.included_modules.each do |_inc_decl, inc_mod|
        yield inc_mod, false
        each_included_module(inc_mod, &blk)
      end
    end

    def get_superclass(singleton, mod)
      super_mod = mod.superclass
      if super_mod
        return [singleton, super_mod]
      else
        if mod == @mod_basic_object
          if singleton
            return [false, @mod_class]
          else
            return nil
          end
        elsif mod == @mod_module && !singleton
          return nil
        else
          return [false, @mod_module]
        end
      end
    end

    def get_instance_type(mod, type_args, changes, base_ty_env, base_ty)
      ty_env = base_ty_env.dup
      if base_ty.is_a?(Type::Instance)
        base_ty.mod.type_params.zip(base_ty.args) do |param, arg|
          ty_env[param] = arg || Source.new
        end
      elsif base_ty.is_a?(Type::Singleton)
        base_ty.mod.type_params&.each do |param|
          ty_env[param] = Source.new
        end
      end
      args = mod.type_params.zip(type_args).map do |param, arg|
        arg && changes ? arg.covariant_vertex(self, changes, ty_env) : Source.new
      end
      Type::Instance.new(self, mod, args)
    end

    def get_superclass_type(ty, changes, base_ty_env)
      singleton, super_mod = get_superclass(ty.is_a?(Type::Singleton), ty.mod)
      return unless super_mod

      if singleton
        Type::Singleton.new(self, super_mod)
      else
        get_instance_type(super_mod, ty.mod.superclass_type_args || [], changes, base_ty_env, ty)
      end
    end

    def add_static_eval_queue(change_type, arg)
      @static_eval_queue << [change_type, arg]
    end

    def define_all
      until @static_eval_queue.empty?
        change_type, arg = @static_eval_queue.shift
        case change_type
        when :inner_modules_changed
          arg[0].on_inner_modules_changed(self, arg[1])
        when :static_read_changed
          case arg
          when BaseStaticRead
            arg.on_scope_updated(self)
          when ScopedStaticRead
            arg.on_cbase_updated(self)
          else
            raise
          end
        when :parent_modules_changed
          arg.on_parent_modules_changed(self)
        else
          raise change_type.to_s
        end
      end
    end

    def add_run(obj)
      unless @run_queue_set.include?(obj)
        @run_queue << obj
        @run_queue_set << obj
      end
    end

    def run_all
      run_count = 0
      until @run_queue.empty?
        obj = @run_queue.shift
        @run_queue_set.delete(obj)
        unless obj.destroyed
          run_count += 1
          obj.run(self)
        end
      end
      @run_count += run_count
    end

    def process_diagnostic_paths
      processed_paths = []
      @pending_diagnostic_paths.each do |path|
        processed = yield path
        processed_paths << path if processed
      end
      processed_paths.each {|path| @pending_diagnostic_paths.delete(path) }
    end

    def add_diagnostic_path(path)
      @pending_diagnostic_paths << path unless @pending_diagnostic_paths.include?(path)
    end

    # just for validation
    def get_vertexes(vtxs)
      @mod_object.get_vertexes(vtxs)
      @gvars.each_value do |gvar_entity|
        vtxs << gvar_entity.vtx
      end
    end

    # classes and modules

    def resolve_cpath(cpath)
      mod = @mod_object
      raise unless cpath # annotation
      cpath.each do |cname|
        mod = mod.inner_modules[cname] ||= ModuleEntity.new(mod.cpath + [cname], mod)
      end
      mod
    end

    # constants

    def resolve_const(cpath)
      mod = resolve_cpath(cpath[0..-2])
      mod.get_const(cpath[-1])
    end

    def resolve_method(cpath, singleton, mid)
      mod = resolve_cpath(cpath)
      mod.get_method(singleton, mid)
    end

    def resolve_gvar(name)
      @gvars[name] ||= ValueEntity.new
    end

    def resolve_ivar(cpath, singleton, name)
      # TODO: include はあとで考える
      mod = resolve_cpath(cpath)
      mod.get_ivar(singleton, name)
    end

    def resolve_cvar(cpath, name)
      # TODO: include はあとで考える
      mod = resolve_cpath(cpath)
      mod.get_cvar(name)
    end

    def resolve_type_alias(cpath, name)
      # TODO: include はあとで考える
      mod = resolve_cpath(cpath)
      mod.get_type_alias(name)
    end

    def load_core_rbs(raw_decls)
      lenv = LocalEnv.new(nil, CRef::Toplevel, {}, [])
      decls = raw_decls.map do |raw_decl|
        AST.create_rbs_decl(raw_decl, lenv)
      end.compact

      decls += AST.parse_rbs("typeprof-rbs-shim.rbs", <<-RBS)
        class Exception
          include _Exception
        end
        class String
          include _ToS
          include _ToStr
        end
        class Array[Elem]
          include _ToAry[Elem]
          include _Each[Elem]
        end
        class Hash[K, V]
          include _Each[[K, V]]
        end
      RBS

      # Loading frequently used modules first will reduces constant resolution
      # which makes loading faster :-)
      critical_modules = [
        decls.find {|decl| decl.cpath == [:Object] },
        decls.find {|decl| decl.cpath == [:Module] },
        decls.find {|decl| decl.cpath == [:Numeric] },
        decls.find {|decl| decl.cpath == [:Integer] },
        decls.find {|decl| decl.cpath == [:String] },
        decls.find {|decl| decl.cpath == [:Array] },
        decls.find {|decl| decl.cpath == [:Hash] },
        decls.find {|decl| decl.cpath == [:Enumerator] },
      ]
      decls = critical_modules + (decls - critical_modules)

      decls.each {|decl| decl.define(self) }
      define_all
      decls.each {|decl| decl.install(self) }
      run_all
    end
  end

  class LocalEnv
    def initialize(path, cref, locals, return_boxes)
      @path = path
      @cref = cref
      @locals = locals
      @return_boxes = return_boxes
      @break_vtx = nil
      @next_boxes = []
      @ivar_narrowings = {}
      @strict_const_scope = false
    end

    attr_reader :path, :cref, :locals, :return_boxes, :break_vtx, :next_boxes, :strict_const_scope

    def new_var(name, node)
      @locals[name] = Vertex.new(node)
    end

    def set_var(name, vtx)
      @locals[name] = vtx
    end

    def get_var(name)
      @locals[name] || raise("#{ name }")
    end

    def exist_var?(name)
      !!@locals[name]
    end

    def add_return_box(box)
      @return_boxes << box
    end

    def add_next_box(box)
      @next_boxes << box
    end

    def get_break_vtx
      @break_vtx ||= Vertex.new(:break_vtx)
    end


    def push_ivar_narrowing(name, narrowing)
      raise unless narrowing.is_a?(Narrowing::Constraint)
      (@ivar_narrowings[name] ||= []) << narrowing
    end

    def pop_ivar_narrowing(name)
      (@ivar_narrowings[name] ||= []).pop
    end

    def apply_ivar_narrowing(genv, node, name, vtx)
      if @ivar_narrowings[name] && !@ivar_narrowings[name].empty?
        # Apply all accumulated narrowings in order
        @ivar_narrowings[name].each do |narrowing|
          vtx = narrowing.narrow(genv, node, vtx)
        end
        return vtx
      end
      vtx
    end

    def use_strict_const_scope
      @strict_const_scope = true
      yield
    ensure
      @strict_const_scope = false
    end
  end

  class CRef
    def initialize(cpath, scope_level, mid, outer)
      @cpath = cpath
      @scope_level = scope_level
      @mid = mid
      @outer = outer
    end

    attr_reader :cpath, :scope_level, :mid, :outer

    def get_self(genv)
      case @scope_level
      when :instance
        mod = genv.resolve_cpath(@cpath || [])
        type_params = mod.type_params.map {|ty_param| Source.new() } # TODO: better support
        ty = Type::Instance.new(genv, mod, type_params)
        Source.new(ty)
      when :class
        Source.new(Type::Singleton.new(genv, genv.resolve_cpath(@cpath || [])))
      else
        Source.new()
      end
    end

    Toplevel = self.new([], :instance, nil, nil)
  end
end
