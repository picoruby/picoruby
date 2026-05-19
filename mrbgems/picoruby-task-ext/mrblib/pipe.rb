class Task
  # Task::Pipe - thin IO-shaped wrapper around Task::Queue.
  #
  # $stdout = Task::Pipe.new(queue) inside a Task#fork block redirects
  # all plain puts/print/gets/getc calls to the queue without touching core.
  #
  # IO semantics (matching PipelineIO):
  #   puts   - appends "\n" only when the string does not already end with one
  #   getc   - returns one character; leftovers are buffered in @buf
  #   write  - raw push with no newline
  #   gets   - pop one queue item (returns nil when queue closed)
  #
  # Push on a closed queue raises Task::Error("queue closed") intentionally.
  # This is the SIGPIPE-equivalent: the producer unwinds, the task goes DORMANT,
  # and the pipeline executor proceeds. Suppressing it would leave producers
  # spinning indefinitely.
  class Pipe
    def initialize(queue)
      @q = queue
      @buf = ""
    end

    def puts(*args)
      if args.empty?
        @q.push("\n")
      else
        i = 0
        while i < args.size
          s = args[i].to_s
          if s.empty? || s[-1] != "\n"
            @q.push("#{s}\n")
          else
            @q.push(s)
          end
          i += 1
        end
      end
    end

    def print(*args)
      i = 0
      while i < args.size
        @q.push(args[i].to_s)
        i += 1
      end
    end

    def write(str)
      @q.push(str.to_s)
    end

    def gets
      if @buf && !@buf.empty?
        s = @buf
        @buf = ""
        return s
      end
      @q.pop
    end

    def getc
      if @buf.empty?
        s = @q.pop
        return nil if s.nil?
        @buf = s
      end
      c = @buf[0]
      @buf = @buf[1..-1] || ""
      c
    end

    # Return -1 so Kernel#getc's `$stdin.fileno == 0` check routes to
    # $stdin.getc (real terminal) rather than this pipe.
    def fileno
      -1
    end

    def flush
      # no-op: queue is unbuffered
    end

    def close
      @q.close unless @q.closed?
    end

    def closed?
      @q.closed?
    end
  end
end
