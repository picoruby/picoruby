class SocketError < StandardError; end

class EOFError < IOError; end

class BasicSocket
  CONNECTION_TIMEOUT_MS = 10_000
  READ_TIMEOUT_MS = 60_000

  private def __connection_timeout_ms
    CONNECTION_TIMEOUT_MS
  end

  # A connection that timed out cannot be used, so release its resources here.
  private def __wait_for_event(event_queue, timeout_message)
    unless event_queue.pop(timeout_ms: __connection_timeout_ms)
      close
      raise SocketError, timeout_message
    end
  end

  # A read timeout is recoverable (the peer may just be slow), so keep the
  # socket open and let the caller retry or close.
  private def __readpartial_event_queue(maxlen, timeout_message)
    event_queue = @event_queue
    return __readpartial_poll(maxlen) unless event_queue

    data = read_nonblock(maxlen)
    while data.nil?
      unless event_queue.pop(timeout_ms: READ_TIMEOUT_MS)
        raise SocketError, timeout_message
      end
      data = read_nonblock(maxlen)
    end
    # @type var data: String
    data
  end

  # Blocking waits (TCPServer#accept, UDPSocket#recvfrom) share this loop.
  # The block is the non-blocking probe and returns nil while nothing is
  # ready.
  #
  # Ctrl-C: the shell dispatches SIGINT from its own task and then stops the
  # task blocked here without unwinding it, so the trap handler runs on a
  # different stack and nothing in this frame (locals, ensure) can be relied
  # on to run afterwards. The handler therefore touches only the socket
  # object: it marks it, puts the previous handler back, and closes it
  # (which also wakes the event queue). This side then finds the socket
  # closed and raises Interrupt.
  #
  # A close from another task (e.g. DRb.stop_service) is reported as IOError,
  # like CRuby does for a stream closed in another thread.
  private def __wait_interruptible
    __raise_closed if closed?
    owner = __install_int_handler
    begin
      while !closed?
        result = yield
        return result if result
        if event_queue = @event_queue
          event_queue.pop
        else
          sleep_ms 10
        end
      end
    ensure
      __restore_int_handler if owner
    end
    __raise_closed
  end

  private def __raise_closed
    raise Interrupt if @interrupted
    raise IOError, "closed stream"
  end

  # Returns true when this call installed the handler. accept_loop installs
  # it once for its whole lifetime, so that Ctrl-C while the block handles a
  # client still closes the listening socket; the nested accept then leaves
  # it alone.
  private def __install_int_handler
    return false if @int_handler
    handler = __build_int_handler
    @int_handler = handler
    @previous_int_handler = Signal.trap(:INT, handler)
    true
  end

  # The Proc is built in a method of its own so that its environment is
  # detached from the task stack by the time it can run: mruby moves a
  # block's captured variables to the heap when the defining method returns.
  # It captures nothing but self on purpose.
  private def __build_int_handler
    Proc.new { __handle_int_signal }
  end

  private def __handle_int_signal
    @interrupted = true
    __restore_int_handler
    close
  end

  # Put the previous handler back only if ours is still the current one;
  # another task may have installed its own trap in the meantime and that
  # one must survive. Signal.trap is the only way to read the current
  # handler, hence the swap. Identity goes through object_id because
  # mruby/c has no equal?.
  private def __restore_int_handler
    handler = @int_handler
    return unless handler
    @int_handler = nil
    previous = @previous_int_handler
    @previous_int_handler = nil
    # @type var current: untyped
    current = Signal.trap(:INT, previous || "DEFAULT")
    Signal.trap(:INT, current) unless current.object_id == handler.object_id
  end

  # IO-compatible methods

  def read(maxlen = nil)
    raise TypeError, "no implicit conversion into Integer" unless maxlen.nil? || maxlen.is_a?(Integer)
    if maxlen.nil?
      res = ''
      begin
        while true
          res << readpartial(100)
        end
      rescue EOFError
      end
      return res
    elsif maxlen < 0
      raise ArgumentError, "negative length #{maxlen} given"
    elsif maxlen == 0
      return ''
    else
      res = ''
      remaining = maxlen
      begin
        while 0 < remaining
          chunk = readpartial(remaining)
          res << chunk
          remaining -= chunk.bytesize
        end
      rescue EOFError
      end
      return res.empty? ? nil : res
    end
  end

  def write(*str_ary)
    write_len = 0
    i = 0
    str_ary_len = str_ary.length
    while i < str_ary_len
      str = str_ary[i].to_s
      offset = 0
      str_len = str.bytesize
      while offset < str_len
        rest = str.byteslice(offset, str_len - offset)
        raise RuntimeError, "write failed" if rest.nil? || rest.empty?
        sent = send(rest, 0)
        raise RuntimeError, "write failed" if sent <= 0
        offset += sent
        write_len += sent
      end
      i += 1
    end
    write_len
  end

  def puts(*args)
    if args.length == 0
      write("\n")
      return nil
    end
    i = 0
    args_len = args.length
    while i < args_len
      arg_str = args[i]&.to_s
      write(arg_str) if arg_str
      write("\n") unless arg_str&.end_with?("\n")
      i += 1
    end
    nil
  end

  def gets(sep = "\n")
    buffer = ""
    begin
      while true
        buffer << readpartial(1)
        break if buffer.end_with?(sep)
      end
    rescue EOFError
      return nil if buffer.empty?
    end
    buffer
  end

  def print(*args)
    i = 0
    args_len = args.length
    while i < args_len
      write(args[i].to_s)
      i += 1
    end
    nil
  end

  def eof?
    closed?
  end

  def peeraddr
    # Returns [address_family, port, hostname, numeric_address]
    ["AF_INET", remote_port, remote_host, remote_host]
  end

  def remote_address
    # Returns simplified Addrinfo
    "#{remote_host}:#{remote_port}"
  end

  def local_address
    # Not implemented yet
    nil
  end
end
