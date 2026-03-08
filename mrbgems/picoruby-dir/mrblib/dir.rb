require 'env'

class Dir
  include Enumerable

  def each(&block)
    # @type var block: ^(String) -> void
    while s = self.read
      block.call(s)
    end
    self
  end

  alias each_child each
  alias pos tell
  alias pos= seek

  class << self
    def entries(path)
      a = [] #: Array[String]
      self.open(path) do |d|
        while s = d.read
          a << s
        end
      end
      a
    end
    alias children entries

    def foreach(path, &block)
      raise ArgumentError, "no block given" unless block
      self.open(path) do |d|
        d.each(&block)
      end
      nil
    end

    def open(path, &block)
      if block
        d = self.new(path)
        begin
          # steep bug: `class << self` causes `instance` type to resolve to `singleton(Dir)` instead of `Dir`
          block.call(d) # steep:ignore ArgumentTypeMismatch
        ensure
          begin
            d.close
          rescue IOError
          end
        end
      else
        self.new(path)
      end
    end

    def chdir(path = nil, &block)
      path ||= ENV['HOME']
      if path.nil? || path.empty?
        raise ArgumentError, "No home directory"
      end
      if block
        wd = self.getwd
        begin
          self._chdir(path)
          block.call(path)
        ensure
          self._chdir(wd)
        end
      else
        self._chdir(path)
      end
    end

    alias rmdir delete
  end
end

