class UDPSocket
  # UDPSocket is already defined in C
  # This file adds convenience methods

  # Receive data with blocking behavior
  # Continuously polls recvfrom_nonblock until data is available
  def recvfrom(maxlen, flags = 0)
    Signal.trap(:INT) do
      self.close
    end
    while true
      result = recvfrom_nonblock(maxlen, flags)
      break if result
      sleep_ms 10
    end
    # @type var result: [String, Array[String | Integer]]
    return result
  end

  # Read data from any source (simplified version of recvfrom)
  # Incompatible with CRuby but maxlen should be specified in restricted environments
  def read(maxlen)
    data, _addr = recvfrom(maxlen)
    data
  end

  # Check if socket is at end of file (UDP doesn't have EOF concept)
  def eof?
    closed?
  end

  def gets(separator = "\n")
    raise NotImplementedError, "UDPSocket does not support gets method"
  end

  def readpartial(maxlen)
    raise NotImplementedError, "UDPSocket does not support readpartial method"
  end

  def read_nonblock(maxlen)
    raise NotImplementedError, "UDPSocket does not support read_nonblock method"
  end
end
