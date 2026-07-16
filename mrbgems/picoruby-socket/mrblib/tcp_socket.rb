class TCPSocket < BasicSocket
  # TCPSocket is mostly implemented in C
  # This file provides additional Ruby-level methods

  alias __readpartial_poll readpartial

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

    while true
      data = read_nonblock(maxlen)
      return data if data
      event_queue.pop
    end
  end
end
