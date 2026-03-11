module TypeProf::Core
  class TypeAliasEntity
    def initialize
      @decls = Set[]
      @type = nil
    end

    attr_reader :decls, :type

    def exist?
      !@decls.empty?
    end

    def add_decl(decl)
      @decls << decl
      @type = decl.type unless @type
      # TODO: report an error if there are duplicated declarations
    end

    def remove_decl(decl)
      @decls.delete(decl) || raise
      if @type == decl.type
        @type = @decls.empty? ? nil : @decls.to_a.first.type
      end
    end
  end
end
