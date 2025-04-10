require 'env'

class Dir
  include Enumerable

  def each(&block)
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
      a = []
      self.open(path) do |d|
        while s = d.read
          a << s
        end
      end
      a
    end
    alias children entries

    def foreach(path, &block)
      self.open(path) do |d|
        d.each(&block)
      end
      nil
    end

    def open(path, &block)
      if block
        d = self.new(path)
        begin
          block.call(d)
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

