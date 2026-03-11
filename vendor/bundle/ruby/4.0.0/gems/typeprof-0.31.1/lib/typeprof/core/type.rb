module TypeProf::Core
  class Type
    # This new method does memoize creation of types
    #: (GlobalEnv, *untyped) -> instance
    def self.new(genv, *args)
      genv.type_table[[self] + args] ||= super(genv, *args)
    end

    def self.strip_parens(s)
      #s =~ /\A\((.*)\)\z/ ? $1 : s
      s.start_with?("(") && s.end_with?(")") ? s[1..-2] || raise : s
    end

    def self.strip_array(s)
      s.start_with?("Array[") && s.end_with?("]") ? s[6..-2] || raise : s
    end

    def self.extract_hash_value_type(s)
      if s.start_with?("Hash[") && s.end_with?("]")
        type = RBS::Parser.parse_type(s)

        if type.is_a?(RBS::Types::Union)
          type.types.map {|t| t.args[1].to_s }.join(" | ")
        else
          type.args[1].to_s
        end
      else
        s
      end
    end

    def self.default_param_map(genv, ty)
      ty = ty.base_type(genv)
      instance_ty = ty.is_a?(Type::Instance) ? ty : Type::Instance.new(genv, ty.mod, []) # TODO: type params
      singleton_ty = ty.is_a?(Type::Instance) ? Type::Singleton.new(genv, ty.mod) : ty
      {
        "*self": Source.new(ty),
        "*instance": Source.new(instance_ty),
        "*class": Source.new(singleton_ty),
      }
    end

    class Singleton < Type
      #: (GlobalEnv, ModuleEntity) -> void
      def initialize(genv, mod)
        raise unless mod.is_a?(ModuleEntity)
        # TODO: type_param
        @mod = mod
      end

      attr_reader :mod

      def base_type(_)
        self
      end

      def show
        "singleton(#{ @mod.show_cpath })"
      end

      def get_instance_type(genv)
        params = @mod.type_params
        Instance.new(genv, @mod, params ? params.map { Source.new } : [])
      end
    end

    class Instance < Type
      #: (GlobalEnv, ModuleEntity, ::Array[Vertex]) -> void
      def initialize(genv, mod, args)
        raise mod.class.to_s unless mod.is_a?(ModuleEntity)
        @mod = mod
        @args = args
        raise unless @args.is_a?(::Array)
      end

      attr_reader :mod, :args

      def base_type(_)
        self
      end

      def show
        case @mod.cpath
        when [:NilClass] then "nil"
        when [:TrueClass] then "true"
        when [:FalseClass] then "false"
        else
          "#{ @mod.show_cpath }#{ @args.empty? ? "" : "[#{ @args.map {|arg| Type.strip_parens(arg.show) }.join(", ") }]" }"
        end
      end
    end

    class Array < Type
      #: (GlobalEnv, ::Array[Vertex], Instance) -> void
      def initialize(genv, elems, base_type)
        @elems = elems
        @base_type = base_type
        raise unless base_type.is_a?(Instance)
      end

      attr_reader :elems

      def get_elem(genv, idx = nil)
        if idx && @elems
          @elems[idx] || Source.new(genv.nil_type)
        else
          @base_type.args.first
        end
      end

      def splat_assign(genv, lefts, rest_elem, rights)
        edges = []
        state = :left
        j = nil
        rights_size = rights ? rights.size : 0
        @elems.each_with_index do |elem, i|
          case state
          when :left
            if i < lefts.size
              edges << [elem, lefts[i]]
            else
              break unless rest_elem
              state = :rest
              redo
            end
          when :rest
            if @elems.size - i > rights_size
              edges << [elem, rest_elem]
            else
              state = :right
              j = i
              redo
            end
          when :right
            edges << [elem, rights[i - j]]
          end
        end
        edges
      end

      def base_type(genv)
        @base_type
      end

      def show
        if @elems
          "[#{ @elems.map {|e| Type.strip_parens(e.show) }.join(", ") }]"
        else
          "#{ @base_type.mod.show_cpath }[#{ Type.strip_parens(@unified_elem.show) }]"
        end
      end
    end

    class Hash < Type
      #: (GlobalEnv, ::Array[Vertex], Instance) -> void
      def initialize(genv, literal_pairs, base_type)
        @literal_pairs = literal_pairs
        @base_type = base_type
        raise unless base_type.is_a?(Instance)
      end

      def get_key
        @base_type.args[0]
      end

      def get_value(key = nil)
        @literal_pairs[key] || @base_type.args[1]
      end

      def base_type(genv)
        @base_type
      end

      def show
        @base_type.show
      end
    end

    class Proc < Type
      def initialize(genv, block)
        @block = block
      end

      attr_reader :block

      def base_type(genv)
        genv.proc_type
      end

      def show
        "<Proc>"
      end
    end

    class Symbol < Type
      #: (GlobalEnv, ::Symbol) -> void
      def initialize(genv, sym)
        @sym = sym
      end

      attr_reader :sym

      def base_type(genv)
        genv.symbol_type
      end

      def show
        @sym.inspect
      end
    end

    class Bot < Type
      def initialize(genv)
      end

      def base_type(genv)
        genv.obj_type
      end

      def show
        "bot"
      end
    end

    class Var < Type
      #: (GlobalEnv, ::Symbol, Vertex) -> void
      def initialize(genv, name, vtx)
        @name = name
        @vtx = vtx
      end

      attr_reader :name, :vtx

      def base_type(genv)
        genv.obj_type # Is this ok?
      end

      def show
        "var[#{ @name }]"
      end
    end

    class Record < Type
      #: (GlobalEnv, ::Hash[Symbol, Vertex], Instance) -> void
      def initialize(genv, fields, base_type)
        @fields = fields
        @base_type = base_type
        raise unless base_type.is_a?(Instance)
      end

      attr_reader :fields

      def get_value(key = nil)
        if key
          # Return specific field value if it exists
          @fields[key]
        elsif @fields.empty?
          # Empty record has no values
          nil
        else
          # Return union of all field values if no specific key
          @base_type.args[1]
        end
      end

      def base_type(genv)
        @base_type
      end

      def show
        field_strs = @fields.map do |key, val_vtx|
          "#{ key }: #{ Type.strip_parens(val_vtx.show) }"
        end
        "{ #{ field_strs.join(", ") } }"
      end
    end
  end
end
