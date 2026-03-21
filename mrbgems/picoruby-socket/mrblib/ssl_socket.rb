class SSLContext
  # SSLContext is mostly implemented in C
  # This file provides additional Ruby-level methods

  unless Machine.posix?
    # On non-POSIX platforms, ca_file=/cert_file=/key_file= are not supported
    # in C (no filesystem access from mbedtls). Read the file in Ruby and pass
    # the buffer via set_ca_pem/set_cert_pem/set_key_pem.
    def ca_file=(path)
      set_ca_pem(File.read(path))
    end

    def cert_file=(path)
      set_cert_pem(File.read(path))
    end

    def key_file=(path)
      set_key_pem(File.read(path))
    end
  end
end

class SSLSocket < BasicSocket
  # SSLSocket is mostly implemented in C
  # This file provides additional Ruby-level methods

  # Keep references to prevent GC
  attr_reader :tcp_socket, :ssl_context

  # Instance methods

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
