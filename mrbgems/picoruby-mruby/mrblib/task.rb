class Task
  attr_accessor :name

  class Stat
    def [](key)
      @data[key]
    end
  end
end
