module TypeProf::Core
  class Narrowing
    def initialize(map)
      raise unless map.is_a?(Hash)
      @map = map
    end

    attr_reader :map

    def and(other)
      new_map = @map.dup
      other.map.each do |var, constraint|
        new_map[var] = new_map[var] ? new_map[var].and(constraint) : constraint
      end
      Narrowing.new(new_map)
    end

    def or(other)
      new_map = {}
      @map.each do |var, constraint|
        new_map[var] = constraint.or(other.map[var]) if other.map[var]
      end
      Narrowing.new(new_map)
    end

    EmptyNarrowings = [Narrowing.new({}), Narrowing.new({})]

    # Narrowing system for type refinement
    class Constraint
      def and(other)
        AndConstraint.new(self, other)
      end

      def or(other)
        OrConstraint.new(self, other)
      end
    end

    class IsAConstraint < Constraint
      def initialize(arg, neg)
        @arg = arg
        @neg = neg
      end

      attr_reader :arg, :neg

      def negate
        IsAConstraint.new(@arg, !@neg)
      end

      def inspect
        @neg ? "!#{@arg}" : "#{@arg}"
      end

      def narrow(genv, node, vtx)
        if @arg.static_ret
          IsAFilter.new(genv, node, vtx, @neg, @arg.static_ret).next_vtx
        else
          vtx
        end
      end
    end

    class NilConstraint < Constraint
      def initialize(neg)
        @neg = neg
      end

      attr_reader :neg

      def inspect
        @neg ? "!nil" : "nil"
      end

      def negate
        NilConstraint.new(!@neg)
      end

      def narrow(genv, node, vtx)
        NilFilter.new(genv, node, vtx, @neg).next_vtx
      end
    end

    class AndConstraint < Constraint
      def initialize(left, right)
        @left = left
        @right = right
      end

      attr_reader :left, :right

      def inspect
        "(#{@left.inspect} & #{@right.inspect})"
      end

      def negate
        OrConstraint.new(@left.negate, @right.negate)
      end

      def narrow(genv, node, vtx)
        @left.narrow(genv, node, @right.narrow(genv, node, vtx))
      end
    end

    class OrConstraint < Constraint
      def initialize(left, right)
        @left = left
        @right = right
      end

      attr_reader :left, :right

      def inspect
        "(#{@left.inspect} | #{@right.inspect})"
      end

      def negate
        AndConstraint.new(@left.negate, @right.negate)
      end

      def narrow(genv, node, vtx)
        ret = Vertex.new(node)
        vtx1 = @left.narrow(genv, node, vtx)
        vtx2 = @right.narrow(genv, node, vtx)
        vtx1.add_edge(genv, ret)
        vtx2.add_edge(genv, ret)
        ret
      end
    end
  end
end
