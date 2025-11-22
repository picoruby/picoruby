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
  def read(maxlen = 4096)
    data, _addr = recvfrom(maxlen)
    data
  end

  # Write data to connected destination
  def write(data)
    send(data.to_s, 0)
  end

  # Check if socket is at end of file (UDP doesn't have EOF concept)
  def eof?
    closed?
  end
end
