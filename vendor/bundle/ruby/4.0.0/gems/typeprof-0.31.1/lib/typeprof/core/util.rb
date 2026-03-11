module TypeProf::Core
  class Set
    def self.[](*elems)
      h = Hash.new(false)
      elems.each {|elem| h[elem] = true }
      new(h)
    end

    def initialize(hash)
      @hash = hash
    end

    def internal_hash = @hash

    def dup
      Set.new(@hash.dup)
    end

    def <<(elem)
      raise if @hash.include?(elem)
      @hash[elem] = true
      self
    end

    def include?(elem)
      @hash[elem]
    end

    def merge(set)
      raise NotImplementedError
    end

    def each(&blk)
      @hash.each_key(&blk)
    end

    def empty?
      @hash.empty?
    end

    def delete(elem)
      raise unless @hash.include?(elem)
      @hash.delete(elem)
    end

    def clear
      @hash.clear
    end

    def to_a
      @hash.keys
    end

    def size
      @hash.size
    end

    def -(other)
      h = @hash.dup
      other.each do |elem|
        h.delete(elem)
      end
      Set.new(h)
    end

    def pretty_print(q)
      q.text "Set["
      q.group do
        q.nest(1) do
          @hash.each_key do |elem|
            q.breakable ""
            q.pp elem
            q.text ","
          end
        end
        q.breakable ""
      end
      q.text "]"
    end
  end
end
