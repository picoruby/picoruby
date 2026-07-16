class TCPSocket < BasicSocket
  # TCPSocket is mostly implemented in C
  # This file provides additional Ruby-level methods

  def initialize(host, port)
    __initialize_poll(host, port)
    event_queue = @event_queue
    return unless event_queue

    while __connection_state == 1
      unless event_queue.pop(timeout_ms: 10_000)
        close
        raise SocketError, "connection timed out"
      end
    end
    return if __connection_state == 2

    message = __error_message
    close
    raise SocketError, message || "failed to connect"
  end

  # Class methods

  def self.open(host, port)
    new(host, port)
  end

  def self.gethostbyname(host)
    # Simplified version - returns the host as-is
    # In a full implementation, this would do actual DNS resolution
    [host, [], 2, host]
  end

  # Instance methods

  def addr
    # Returns [address_family, port, hostname, numeric_address]
    # For local address (not yet implemented, returns placeholder)
    ["AF_INET", 0, "0.0.0.0", "0.0.0.0"]
  end

  def connected?
    !closed?
  end

  def readpartial(maxlen)
    event_queue = @event_queue
    return __readpartial_poll(maxlen) unless event_queue

    data = read_nonblock(maxlen)
    until data
      event_queue.pop
      data = read_nonblock(maxlen)
    end
    data || raise(IOError, "read failed")
  end
end
