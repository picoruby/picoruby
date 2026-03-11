module TypeProf::Core
  class StaticRead
    def initialize(name)
      @name = name
      @followers = Set[]
      @source_modules = Set[]
    end

    attr_reader :name, :followers

    def propagate(genv)
      @followers.each do |follower|
        case follower
        when ModuleEntity
          genv.add_static_eval_queue(:parent_modules_changed, follower)
        when ScopedStaticRead
          follower.on_cbase_updated(genv)
        when Box, IsAFilter
          genv.add_run(follower)
        else
          raise follower.inspect
        end
      end
    end

    def destroy(genv)
      @source_modules.each do |mod|
        mod.static_reads[@name].delete(self) || raise
      end
      @source_modules.clear
    end

    def resolve(genv, cref, search_ancestors, break_object)
      destroy(genv)

      while cref
        scope = cref.cpath
        mod = genv.resolve_cpath(scope)
        genv.each_superclass(mod, false) do |mod, _singleton|
          break if mod == genv.mod_object && break_object

          unless @source_modules.include?(mod)
            @source_modules << mod
            (mod.static_reads[@name] ||= Set[]) << self
          end

          return if check_module(genv, mod)

          break unless search_ancestors
        end
        search_ancestors = false
        cref = cref.outer
      end
      resolution_failed(genv)
    end
  end

  class BaseStaticRead < StaticRead
    def initialize(genv, name, cref, strict_const_scope)
      super(name)
      @cref = cref
      @search_ancestors = !strict_const_scope
      genv.add_static_eval_queue(:static_read_changed, self)
    end

    attr_reader :cref

    def on_scope_updated(genv)
      resolve(genv, @cref, @search_ancestors, false)
    end
  end

  class ScopedStaticRead < StaticRead
    def initialize(name, cbase, strict_const_scope)
      super(name)
      @cbase = cbase
      @cbase.followers << self if @cbase
      @search_ancestors = !strict_const_scope
    end

    def on_cbase_updated(genv)
      if @cbase && @cbase.cpath
        resolve(genv, CRef.new(@cbase.cpath, :class, nil, nil), @search_ancestors, true)
      else
        resolution_failed(genv)
      end
    end
  end

  module ConstRead
    def check_module(genv, mod)
      cdef = mod.consts[@name]
      if cdef && cdef.exist?
        inner_mod = genv.resolve_cpath(mod.cpath + [@name]) # TODO
        cpath = inner_mod.exist? ? inner_mod.cpath : nil
        update_module(genv, cpath, cdef)
        return true
      end
      return false
    end

    def resolution_failed(genv)
      update_module(genv, nil, nil)
    end

    def update_module(genv, cpath, cdef)
      if cpath != @cpath || cdef != @cdef
        @cpath = cpath
        @cdef = cdef
        propagate(genv)
      end
    end

    attr_reader :cpath, :cdef
  end

  class BaseConstRead < BaseStaticRead
    include ConstRead
  end

  class ScopedConstRead < ScopedStaticRead
    include ConstRead
  end

  module TypeAliasRead
    def check_module(genv, mod)
      tae = mod.type_aliases[@name]
      if tae && tae.exist?
        update_type_alias(genv, tae)
        return true
      end
      return false
    end

    def resolution_failed(genv)
      update_type_alias(genv, nil)
    end

    def update_type_alias(genv, tae)
      if tae != @type_alias_entity
        @type_alias_entity = tae
        propagate(genv)
      end
    end

    attr_reader :type_alias_entity
  end

  class BaseTypeAliasRead < BaseStaticRead
    include TypeAliasRead
  end

  class ScopedTypeAliasRead < ScopedStaticRead
    include TypeAliasRead
  end
end
