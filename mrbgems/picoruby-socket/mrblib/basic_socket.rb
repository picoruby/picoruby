class SocketError < StandardError; end

class EOFError < IOError; end

class BasicSocket
  # IO-compatible methods

  def read(maxlen = nil)
    raise TypeError, "no implicit conversion into Integer" unless maxlen.nil? || maxlen.is_a?(Integer)
    if maxlen.nil?
      res = ''
      begin
        while true
          res << readpartial(100)
        end
      rescue EOFError
      end
      return res
    elsif maxlen < 0
      raise ArgumentError, "negative length #{maxlen} given"
    elsif maxlen == 0
      return ''
    else
      res = ''
      remaining = maxlen
      begin
        while 0 < remaining
          chunk = readpartial(remaining)
          res << chunk
          remaining -= chunk.bytesize
        end
      rescue EOFError
      end
      return res.empty? ? nil : res
    end
  end

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
    if args.length == 0
      send("\n", 0)
      return nil
    end
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
    begin
      while true
        buffer << readpartial(1)
        break if buffer.end_with?(sep)
      end
    rescue EOFError
      return nil if buffer.empty?
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
