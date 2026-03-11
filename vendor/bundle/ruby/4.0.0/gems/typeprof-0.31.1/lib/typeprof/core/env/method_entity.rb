module TypeProf::Core
  class MethodEntity
    def initialize
      @builtin = nil
      @decls = Set[]
      @overloading_decls = Set[]
      @defs = Set[]
      @aliases = {}
      @method_call_boxes = Set[]
    end

    attr_reader :decls, :defs, :aliases, :method_call_boxes
    attr_accessor :builtin

    def add_decl(decl)
      if decl.overloading
        @overloading_decls << decl
      else
        @decls << decl
      end
    end

    def remove_decl(decl)
      if decl.overloading
        @overloading_decls.delete(decl) || raise
      else
        @decls.delete(decl) || raise
      end
    end

    def add_def(mdef)
      @defs << mdef
      self
    end

    def remove_def(mdef)
      @defs.delete(mdef) || raise
    end

    def add_alias(node, old_mid)
      @aliases[node] = old_mid
    end

    def remove_alias(node)
      @aliases.delete(node) || raise
    end

    def exist?
      @builtin || !@decls.empty? || !@defs.empty?
    end

    def add_run_all_mdefs(genv)
      @defs.each do |mdef|
        genv.add_run(mdef)
      end
    end

    def add_run_all_method_call_boxes(genv)
      @method_call_boxes.each do |box|
        genv.add_run(box)
      end
    end
  end
end
