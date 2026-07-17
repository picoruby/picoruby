class SSLContext
  # SSLContext is mostly implemented in C
  # This file provides additional Ruby-level methods

  unless Machine.posix?
    # On non-POSIX platforms, ca_file=/cert_file=/key_file= are not supported
    # in C (no filesystem access from mbedtls). Read the file in Ruby and pass
    # the buffer via set_ca_pem/set_cert_pem/set_key_pem.
    def ca_file=(path)
      set_ca_pem(File.open(path, "r") { |f| f.read })
    end

    def cert_file=(path)
      set_cert_pem(File.open(path, "r") { |f| f.read })
    end

    def key_file=(path)
      set_key_pem(File.open(path, "r") { |f| f.read })
    end
  end
end

class SSLSocket < BasicSocket
  # SSLSocket is mostly implemented in C
  # This file provides additional Ruby-level methods

  # Keep references to prevent GC
  attr_reader :tcp_socket, :ssl_context

  # Instance methods

  if Object.const_defined?(:SocketDNSResolver)
    def self.open(host, port, ssl_context)
      socket = __open_poll(host, port, ssl_context)
      socket.connect
      socket
    end

    def connect
      host = remote_host
      SocketDNSResolver.resolve_host(host)
      __connect_poll
      event_queue = @event_queue
      return self unless event_queue

      while __connection_state == 1
        __wait_for_event(event_queue, "SSL handshake timed out")
      end

      if __connection_state == 2
        return self if __finish_connect

        close
        raise SocketError, "SSL receive buffer allocation failed"
      end

      message = __error_message
      close
      raise SocketError, message || "SSL handshake failed"
    end

    def readpartial(maxlen)
      __readpartial_event_queue(maxlen, "SSL read timeout", "SSL read failed")
    end
  end

  def addr
    # Returns [address_family, port, hostname, numeric_address]
    # Delegates to underlying TCP socket
    ["AF_INET", 0, "0.0.0.0", "0.0.0.0"]
  end

  def connected?
    !closed?
  end

  # SSL-specific placeholder methods
  # These would require additional C implementation for full functionality

  def peer_cert
    # Returns nil for now
    # In a full implementation, this would return the peer's certificate
    nil
  end

  def peer_cert_chain
    # Returns empty array for now
    # In a full implementation, this would return the certificate chain
    []
  end

  def cipher
    # Returns nil for now
    # In a full implementation, this would return cipher info
    nil
  end

  def ssl_version
    # Returns default version for now
    # In a full implementation, this would return the actual TLS version
    "TLSv1.2"
  end

  def state
    # Returns connection state
    connected? ? "connected" : "closed"
  end
end
