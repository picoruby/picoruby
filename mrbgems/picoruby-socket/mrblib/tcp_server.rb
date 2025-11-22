# TCPServer class - CRuby compatible TCP server implementation
#
# Usage:
#   # CRuby compatible
#   server = TCPServer.new(nil, 8080)
#   server = TCPServer.new("127.0.0.1", 8080)
#
#   # PicoRuby extension with backlog
#   server = TCPServer.new(nil, 8080, 10)
#   server = TCPServer.new("127.0.0.1", 8080, 15)
#
#   loop do
#     client = server.accept
#     # handle client...
#     client.close
#   end
#   server.close

class TCPServer
  # Create a new TCP server listening on the specified port
  #
  # CRuby compatible signature with optional backlog extension:
  #   new(host, service)           - Listen on the specified host and port
  #   new(host, service, backlog)  - Full parameters with backlog
  #
  # @param host [String, nil] Hostname or IP address (currently ignored in embedded impl)
  # @param service [Integer] Port number to listen on (1-65535)
  # @param backlog [Integer] Maximum number of pending connections (default: 5)
  # @raise [ArgumentError] if port is invalid or wrong number of arguments
  # @raise [TypeError] if arguments have wrong types
  # @raise [RuntimeError] if server creation fails
  # (Implementation is in C bindings)

  # Accept an incoming client connection (blocking, interruptible)
  #
  # This method blocks until a client connects to the server.
  # Can be interrupted by raising Interrupt exception from another task or Ctrl-C.
  #
  # @return [TCPSocket] Connected client socket
  # @raise [Interrupt] if interrupted by signal or external task
  def accept
    Signal.trap(:INT) do
      self.close
    end
    while true
      client = accept_nonblock
      break if client
      sleep_ms 10
    end
    # @type var client: TCPSocket
    return client
  end

  # Accept an incoming client connection (non-blocking)
  #
  # This method returns immediately. If a client is waiting, returns the client socket.
  # Otherwise returns nil.
  #
  # @return [TCPSocket, nil] Connected client socket or nil if no connection available
  # @raise [RuntimeError] if accept fails
  # (Implementation is in C bindings)

  # Accept connections in a loop, yielding each client to the block
  #
  # This is a convenience method for the common pattern of accepting
  # connections in an infinite loop.
  #
  # @yield [client] Passes each accepted client to the block
  # @yieldparam client [TCPSocket] Connected client socket
  # @return [void] Never returns (infinite loop)
  #
  # Example:
  #   server.accept_loop do |client|
  #     puts "Client connected"
  #     client.write("Hello!\n")
  #     client.close
  #   end
  def accept_loop
    loop do
      client = accept
      yield client
    end
  end

  # Close the server socket
  #
  # After calling this method, the server will no longer accept
  # new connections. Any existing client connections remain open.
  #
  # @return [nil]
  # (Implementation is in C bindings)
end
