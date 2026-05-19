class PipelineIO
  $outboxes = {}
  $inboxes  = {}
  # Per-consumer leftover from a partially consumed queue item. Each queue
  # item produced by PipelineIO.puts/print is a complete line (or a print
  # chunk); getc returns one character and stores the rest here so that
  # subsequent getc / gets calls can drain the same string.
  $inbox_buffers = {}

  def self.connect(producer_pid, consumer_pid)
    q = Task::Queue.new
    $outboxes[producer_pid] = q
    $inboxes[consumer_pid] = q
    $inbox_buffers[consumer_pid] = ""
  end

  def self.close_outbox(producer_pid)
    q = $outboxes[producer_pid]
    q.close if q && !q.closed?
  end

  def self.cleanup(pids)
    i = 0
    while i < pids.size
      $outboxes.delete(pids[i])
      $inboxes.delete(pids[i])
      $inbox_buffers.delete(pids[i])
      i += 1
    end
  end

  # Push semantics:
  # `Task::Queue#push` raises `Task::Error("queue closed")` when the consumer
  # side of this pipe has terminated and the pipeline executor has closed the
  # outbox. We DO NOT rescue that error here. Letting it propagate is the
  # SIGPIPE-equivalent: the producer script unwinds, the sandbox marks the
  # task DORMANT, and the pipeline executor proceeds. Suppressing the error
  # here would leave finite producers spinning and infinite producers (e.g.
  # `tail -f | head -1`) running forever.
  def self.puts(pid, *args)
    q = $outboxes[pid]
    if q
      if args.empty?
        q.push("\n")
      else
        # Mirror IO#puts semantics: only append a newline when the string
        # does not already end with one. Pushing "#{a}\n" unconditionally
        # would double the newline for callers like `puts gets`.
        args.each do |a|
          s = a.to_s
          if s.empty? || s[-1] != "\n"
            q.push("#{s}\n")
          else
            q.push(s)
          end
        end
      end
    else
      if args.empty?
        $stdout.write("\n")
      else
        args.each do |a|
          s = a.to_s
          $stdout.write(s)
          $stdout.write("\n") if s.empty? || s[-1] != "\n"
        end
      end
    end
  end

  def self.print(pid, *args)
    q = $outboxes[pid]
    if q
      args.each { |a| q.push(a.to_s) }
    else
      args.each { |a| $stdout.write(a.to_s) }
    end
  end

  def self.gets(pid)
    q = $inboxes[pid]
    return $stdin.gets unless q
    buf = $inbox_buffers[pid]
    if buf && !buf.empty?
      # A previous getc left part of a line in the buffer. Return that as
      # the next "line"; producers push newline-terminated items so the
      # leftover already ends with \n (or is the tail of a print chunk).
      $inbox_buffers[pid] = ""
      return buf
    end
    q.pop
  end

  def self.getc(pid)
    q = $inboxes[pid]
    return $stdin.getc unless q
    buf = $inbox_buffers[pid] || ""
    if buf.empty?
      s = q.pop
      return nil if s.nil?
      buf = s
    end
    c = buf[0]
    $inbox_buffers[pid] = buf[1..-1] || ""
    c
  end
end
