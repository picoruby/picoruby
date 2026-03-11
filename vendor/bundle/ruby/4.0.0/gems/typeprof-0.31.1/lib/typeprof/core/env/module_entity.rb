module TypeProf::Core
  class ModuleEntity
    def initialize(cpath, outer_module = self)
      @cpath = cpath

      @module_decls = Set[]
      @module_defs = Set[]
      @include_decls = Set[]
      @include_defs = Set[]
      @prepend_decls = []
      @prepend_defs = []

      @inner_modules = {}
      @outer_module = outer_module

      # parent modules (superclass and all modules that I include)
      @superclass = nil
      @self_types = {}
      @included_modules = {}
      @prepended_modules = {}
      @basic_object = @cpath == [:BasicObject]

      # child modules (subclasses and all modules that include me)
      @child_modules = {}

      # class Foo[X, Y, Z] < Bar[A, B, C]
      @superclass_type_args = nil # A, B, C
      @type_params = [] # X, Y, Z

      @consts = {}
      @methods = { true => {}, false => {} }
      @ivars = { true => {}, false => {} }
      @cvars = {}
      @type_aliases = {}

      @static_reads = {}
      @subclass_checks = Set[]
      @ivar_reads = Set[] # should be handled in @ivars ??
      @cvar_reads = Set[]
    end

    attr_reader :cpath
    attr_reader :module_decls
    attr_reader :module_defs

    attr_reader :inner_modules
    attr_reader :outer_module

    attr_reader :superclass
    attr_reader :self_types
    attr_reader :included_modules
    attr_reader :prepended_modules
    attr_reader :child_modules

    attr_reader :superclass_type_args
    attr_reader :type_params

    attr_reader :consts
    attr_reader :methods
    attr_reader :ivars
    attr_reader :cvars
    attr_reader :type_aliases

    attr_reader :static_reads
    attr_reader :subclass_checks
    attr_reader :ivar_reads
    attr_reader :cvar_reads

    def module?
      !@superclass && !@basic_object
    end

    def interface?
      @cpath.last && @cpath.last.start_with?("_")
    end

    def get_cname
      @cpath.empty? ? :Object : @cpath.last
    end

    def exist?
      !@module_decls.empty? || !@module_defs.empty?
    end

    def on_inner_modules_changed(genv, changed_cname)
      @child_modules.each_key do |child_mod|
        child_mod.on_inner_modules_changed(genv, changed_cname)
      end
      if @static_reads[changed_cname]
        @static_reads[changed_cname].each do |static_read|
          genv.add_static_eval_queue(:static_read_changed, static_read)
        end
      end
    end

    def on_module_added(genv)
      return if @cpath.empty?
      unless exist?
        genv.add_static_eval_queue(:inner_modules_changed, [@outer_module, get_cname])
      end
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def on_module_removed(genv)
      return if @cpath.empty?
      genv.add_static_eval_queue(:parent_modules_changed, self)
      unless exist?
        genv.add_static_eval_queue(:inner_modules_changed, [@outer_module, get_cname])
      end
    end

    def add_module_decl(genv, decl)
      on_module_added(genv)

      @module_decls << decl

      if @type_params
        update_type_params if @type_params != decl.params
      else
        @type_params = decl.params
      end

      if decl.is_a?(AST::SigClassNode) && !@superclass_type_args
        @superclass_type_args = decl.superclass_args
      end

      ce = @outer_module.get_const(get_cname)
      ce.add_decl(decl)
      ce
    end

    def remove_module_decl(genv, decl)
      @outer_module.get_const(get_cname).remove_decl(decl)
      @module_decls.delete(decl) || raise

      update_type_params if @type_params == decl.params
      if decl.is_a?(AST::SigClassNode) && @superclass_type_args == decl.superclass_args
        @superclass_type_args = nil
        @module_decls.each do |decl|
          if decl.superclass_args
            @superclass_type_args = decl.superclass_args
            break
          end
        end
      end

      on_module_removed(genv)
    end

    def update_type_params
      @type_params = nil
      @module_decls.each do |decl|
        params = decl.params
        next unless params
        if @type_params
          @type_params = params if (@type_params <=> params) > 0
        else
          @type_params = params
        end
      end
      @type_params ||= []
      # TODO: report an error if there are multiple inconsistent declarations
    end

    def add_module_def(genv, node)
      on_module_added(genv)
      @module_defs << node
      ce = @outer_module.get_const(get_cname)
      ce.add_def(node)
      ce
    end

    def remove_module_def(genv, node)
      @outer_module.get_const(get_cname).remove_def(node)
      @module_defs.delete(node) || raise
      on_module_removed(genv)
    end

    def add_include_decl(genv, node)
      @include_decls << node
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def remove_include_decl(genv, node)
      @include_decls.delete(node) || raise
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def add_include_def(genv, node)
      @include_defs << node
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def remove_include_def(genv, node)
      @include_defs.delete(node) || raise
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def add_prepend_decl(genv, node)
      @prepend_decls << node
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def remove_prepend_decl(genv, node)
      @prepend_decls.delete(node) || raise
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def add_prepend_def(genv, node)
      @prepend_defs << node
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def remove_prepend_def(genv, node)
      @prepend_defs.delete(node) || raise
      genv.add_static_eval_queue(:parent_modules_changed, self)
    end

    def update_parent(genv, origin, old_parent, new_parent_cpath)
      new_parent = new_parent_cpath ? genv.resolve_cpath(new_parent_cpath) : nil
      if old_parent != new_parent
        # check circular inheritance
        mod = new_parent
        while mod
          if mod == self
            # TODO: report an "circular inheritance" error
            new_parent = nil
            break
          end
          mod = mod.superclass
        end

        if old_parent != new_parent
          if old_parent
            set = old_parent.child_modules[self]
            set.delete(origin)
            old_parent.child_modules.delete(self) if set.empty?
          end
          if new_parent
            set = new_parent.child_modules[self] ||= Set[]
            set << origin
          end
          return [new_parent, true]
        end
      end
      return [new_parent, false]
    end

    def find_superclass_const_read
      return nil if @basic_object

      if @module_decls.empty?
        @module_defs.each do |mdef|
          case mdef
          when AST::ClassNode
            if mdef.superclass_cpath
              const_read = mdef.superclass_cpath.static_ret
              return const_read ? const_read.cpath : []
            end
          when AST::SingletonClassNode
            next
          when AST::ModuleNode
            return nil
          else
            raise
          end
        end
      else
        @module_decls.each do |mdecl|
          case mdecl
          when AST::SigClassNode
            if mdecl.superclass_cpath
              const_read = mdecl.static_ret[:superclass_cpath].last
              return const_read ? const_read.cpath : []
            end
          when AST::SigModuleNode, AST::SigInterfaceNode
            return nil
          end
        end
      end

      return []
    end

    def on_parent_modules_changed(genv)
      any_updated = false

      unless @basic_object
        new_superclass_cpath = find_superclass_const_read

        new_superclass, updated = update_parent(genv, :superclass, @superclass, new_superclass_cpath)
        if updated
          @superclass = new_superclass
          any_updated = true
        end
      end

      @module_decls.each do |mdecl|
        case mdecl
        when AST::SigModuleNode
          mdecl.static_ret[:self_types].each_with_index do |const_reads, i|
            key = [mdecl, i]
            new_parent_cpath = const_reads.last.cpath
            new_self_type, updated = update_parent(genv, key, @self_types[key], new_parent_cpath)
            if updated
              if new_self_type
                @self_types[key] = new_self_type
              else
                @self_types.delete(key) || raise
              end
              any_updated = true
            end
          end
        end
      end
      @self_types.delete_if do |(origin_mdecl, origin_idx), old_mod|
        if @module_decls.include?(origin_mdecl)
          false
        else
          _new_self_type, updated = update_parent(genv, [origin_mdecl, origin_idx], old_mod, nil)
          any_updated ||= updated
          true
        end
      end

      @include_decls.each do |idecl|
        new_parent_cpath = idecl.static_ret.last.cpath
        new_parent, updated = update_parent(genv, idecl, @included_modules[idecl], new_parent_cpath)
        if updated
          if new_parent
            @included_modules[idecl] = new_parent
          else
            @included_modules.delete(idecl) || raise
          end
          any_updated = true
        end
      end
      @include_defs.each do |idef|
        new_parent_cpath = idef.static_ret ? idef.static_ret.cpath : nil
        new_parent, updated = update_parent(genv, idef, @included_modules[idef], new_parent_cpath)
        if updated
          if new_parent
            @included_modules[idef] = new_parent
          else
            @included_modules.delete(idef) || raise
          end
          any_updated = true
        end
      end
      @prepend_decls.each do |pdecl|
        new_parent_cpath = pdecl.static_ret.last.cpath
        new_parent, updated = update_parent(genv, pdecl, @prepended_modules[pdecl], new_parent_cpath)
        if updated
          if new_parent
            @prepended_modules[pdecl] = new_parent
          else
            @prepended_modules.delete(pdecl) || raise
          end
          any_updated = true
        end
      end
      @prepend_defs.each do |pdef|
        new_parent_cpath = pdef.static_ret ? pdef.static_ret.cpath : nil
        new_parent, updated = update_parent(genv, pdef, @prepended_modules[pdef], new_parent_cpath)
        if updated
          if new_parent
            @prepended_modules[pdef] = new_parent
          else
            @prepended_modules.delete(pdef) || raise
          end
          any_updated = true
        end
      end
      @included_modules.delete_if do |origin, old_mod|
        if @include_decls.include?(origin) || @include_defs.include?(origin)
          false
        else
          _new_parent, updated = update_parent(genv, origin, old_mod, nil)
          any_updated ||= updated
          true
        end
      end
      @prepended_modules.delete_if do |origin, old_mod|
        if @prepend_decls.include?(origin) || @prepend_defs.include?(origin)
          false
        else
          _new_parent, updated = update_parent(genv, origin, old_mod, nil)
          any_updated ||= updated
          true
        end
      end

      if any_updated
        @subclass_checks.each do |mcall_box|
          genv.add_run(mcall_box)
        end
        on_ancestors_updated(genv, nil)
      end
    end

    def on_ancestors_updated(genv, base_mod)
      @child_modules.each_key {|child_mod| child_mod.on_ancestors_updated(genv, base_mod || self) }
      @static_reads.each_value do |static_reads|
        static_reads.each do |static_read|
          genv.add_static_eval_queue(:static_read_changed, static_read)
        end
      end
      @methods.each do |_, methods|
        methods.each_value do |me|
          me.method_call_boxes.each do |box|
            genv.add_run(box)
          end
        end
      end
      @ivar_reads.each {|ivar_read| genv.add_run(ivar_read) }
      @cvar_reads.each {|cvar_read| genv.add_run(cvar_read) }
    end

    def each_descendant(base_mod = nil, &blk)
      return if base_mod == self
      yield self
      @child_modules.each_key do |child_mod|
        child_mod.each_descendant(base_mod || self, &blk)
      end
    end

    def get_const(cname)
      @consts[cname] ||= ValueEntity.new
    end

    def get_method(singleton, mid)
      @methods[singleton][mid] ||= MethodEntity.new
    end

    def get_ivar(singleton, name)
      @ivars[singleton][name] ||= ValueEntity.new
    end

    def get_cvar(name)
      @cvars[name] ||= ValueEntity.new
    end

    def get_type_alias(name)
      @type_aliases[name] ||= TypeAliasEntity.new
    end

    def get_vertexes(vtxs)
      @inner_modules.each_value do |mod|
        next if self.equal?(mod) # for Object
        mod.get_vertexes(vtxs)
      end
      @consts.each_value do |cdef|
        vtxs << cdef.vtx
      end
    end

    def show_cpath
      @cpath.empty? ? "Object" : @cpath.join("::" )
    end

    def pretty_print(q)
      q.text "#<ModuleEntity[::#{ @cpath.empty? ? "Object" : @cpath.join("::") }]>"
    end
  end
end
