class BasicSocket
  # IO-compatible methods

  def puts(*args)
    args.each do |arg|
      write(arg.to_s)
      write("\n") unless arg.to_s.end_with?("\n")
    end
    nil
  end

  def gets(sep = "\n")
    buffer = ""
    loop do
      chunk = read(1)
      return nil if chunk.nil? || chunk.empty?
      buffer << chunk
      break if buffer.end_with?(sep)
    end
    buffer
  end

  def print(*args)
    args.each do |arg|
      write(arg.to_s)
    end
    nil
  end

  def eof?
    closed?
  end

  # Socket-specific methods

  def send(data, flags)
    # For now, ignore flags (not supported in basic implementation)
    write(data)
  end

  def recv(maxlen, flags = 0)
    # For now, ignore flags (not supported in basic implementation)
    read(maxlen)
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
