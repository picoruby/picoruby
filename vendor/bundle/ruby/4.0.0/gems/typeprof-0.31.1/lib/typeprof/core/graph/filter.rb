module TypeProf::Core
  class Filter
    def destroyed
      false
    end
  end

  class NilFilter < Filter
    def initialize(genv, node, prev_vtx, allow_nil)
      @node = node
      @next_vtx = Vertex.new(node)
      @allow_nil = allow_nil
      prev_vtx.add_edge(genv, self)
    end

    attr_reader :next_vtx, :allow_nil

    def filter(types, nil_type)
      types.select {|ty| (ty == nil_type) == @allow_nil }
    end

    def on_type_added(genv, src_var, added_types)
      types = filter(added_types, genv.nil_type)
      @next_vtx.on_type_added(genv, self, types) unless types.empty?
    end

    def on_type_removed(genv, src_var, removed_types)
      types = filter(removed_types, genv.nil_type)
      @next_vtx.on_type_removed(genv, self, types) unless types.empty?
    end

    #@@new_id = 0

    def to_s
      "NF#{ @id ||= $new_id += 1 } -> #{ @next_vtx }"
    end
  end

  class IsAFilter < Filter
    def initialize(genv, node, prev_vtx, neg, const_read)
      @node = node
      @types = Set[]
      @const_read = const_read
      @const_read.followers << self
      @next_vtx = Vertex.new(node)
      prev_vtx.add_edge(genv, self)
      @neg = neg
    end

    attr_reader :next_vtx

    def on_type_added(genv, src_var, added_types)
      added_types.each do |ty|
        @types << ty
      end
      run(genv)
    end

    def on_type_removed(genv, src_var, removed_types)
      removed_types.each do |ty|
        @types.delete(ty) || raise
      end
      run(genv)
    end

    def run(genv)
      if @const_read && @const_read.cpath
        passed_types = []
        @types.each do |ty|
          base_ty = ty.base_type(genv)
          subclass = false
          genv.each_superclass(base_ty.mod, base_ty.is_a?(Type::Singleton)) do |mod, singleton|
            if mod.cpath == @const_read.cpath
              subclass = true
              break
            end
          end
          passed_types << ty if subclass != @neg
        end
      else
        passed_types = @types.to_a
      end
      added_types = passed_types - @next_vtx.types.keys
      removed_types = @next_vtx.types.keys - passed_types
      @next_vtx.on_type_added(genv, self, added_types)
      @next_vtx.on_type_removed(genv, self, removed_types)
    end

    #@@new_id = 0

    def to_s
      "NF#{ @id ||= $new_id += 1 } -> #{ @next_vtx }"
    end
  end

  class BotFilter < Filter
    def initialize(genv, node, prev_vtx, base_vtx)
      @node = node
      @types = {}
      @prev_vtx = prev_vtx
      @next_vtx = Vertex.new(node)
      @base_vtx = base_vtx
      base_vtx.add_edge(genv, self)
      prev_vtx.add_edge(genv, self) if prev_vtx != base_vtx
    end

    attr_reader :types, :prev_vtx, :next_vtx, :base_vtx

    def filter(types)
      types.select {|ty| (ty == genv.nil_type) == @allow_nil }
    end

    def on_type_added(genv, src_var, added_types)
      if src_var == @base_vtx
        if @base_vtx.types.size == 1 && @base_vtx.types.include?(Type::Bot.new(genv))
          @next_vtx.on_type_removed(genv, self, @types.keys & @next_vtx.types.keys) # XXX: smoke/control/bot2.rb
        end
      else
        added_types.each do |ty|
          @types[ty] = true
        end
        if @base_vtx.types.size == 1 && @base_vtx.types.include?(Type::Bot.new(genv))
          # ignore
        else
          @next_vtx.on_type_added(genv, self, added_types - @next_vtx.types.keys) # XXX: smoke/control/bot4.rb
        end
      end
    end

    def on_type_removed(genv, src_var, removed_types)
      if src_var == @base_vtx
        if @base_vtx.types.size == 1 && @base_vtx.types.include?(Type::Bot.new(genv))
          # ignore
        else
          @next_vtx.on_type_added(genv, self, @types.keys - @next_vtx.types.keys) # XXX: smoke/control/bot4.rb
        end
      else
        removed_types.each do |ty|
          @types.delete(ty) || raise
        end
        if @base_vtx.types.size == 1 && @base_vtx.types.include?(Type::Bot.new(genv))
          # ignore
        else
          @next_vtx.on_type_removed(genv, self, removed_types & @next_vtx.types.keys) # XXX: smoke/control/bot2.rb
        end
      end
    end

    #@@new_id = 0

    def to_s
      "BF#{ @id ||= $new_id += 1 } -> #{ @next_vtx }"
    end
  end
end
