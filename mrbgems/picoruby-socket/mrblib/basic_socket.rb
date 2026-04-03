class SocketError < StandardError; end

class BasicSocket
  O_NONBLOCK = 1

  # IO-compatible methods

  def write(*str_ary)
    write_len = 0
    i = 0
    str_ary_len = str_ary.length
    while i < str_ary_len
      write_len += send(str_ary[i].to_s, 0)
      i += 1
    end
    write_len
  end

  def puts(*args)
    i = 0
    args_len = args.length
    while i < args_len
      arg_str = args[i]&.to_s
      send(arg_str, 0) if arg_str
      send("\n", 0) unless arg_str&.end_with?("\n")
      i += 1
    end
    nil
  end

  def gets(sep = "\n")
    buffer = ""
    while true
      chunk = read(1)
      return nil if chunk.nil? || chunk.empty?
      buffer << chunk
      break if buffer.end_with?(sep)
    end
    buffer
  end

  def print(*args)
    i = 0
    args_len = args.length
    while i < args_len
      send(args[i].to_s, 0)
      i += 1
    end
    nil
  end

  def eof?
    closed?
  end

  # Socket-specific methods

  def recv(maxlen, flags = 0)
    read(maxlen, flags)
  end

  def recv_nonblock(maxlen, flags = 0)
    read(maxlen, O_NONBLOCK | flags)
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
