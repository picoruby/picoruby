class TCPSocket < BasicSocket
  # TCPSocket is mostly implemented in C
  # This file provides additional Ruby-level methods

  if Object.const_defined?(:SocketDNSResolver)
    def initialize(host, port)
      host = SocketDNSResolver.resolve_host(host)
      __initialize_poll(host, port)
      event_queue = @event_queue
      return unless event_queue

      while __connection_state == 1
        __wait_for_event(event_queue, "connection timed out")
      end
      return if __connection_state == 2

      message = __error_message
      close
      raise SocketError, message || "failed to connect"
    end
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

  if Object.const_defined?(:SocketDNSResolver)
    def readpartial(maxlen)
      __readpartial_event_queue(maxlen, "read timeout", "read failed")
    end
  end
end
