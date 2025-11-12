class UDPSocket
  # UDPSocket is already defined in C
  # This file adds convenience methods

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
