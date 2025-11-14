# TCPServer class - CRuby compatible TCP server implementation
#
# Usage:
#   server = TCPServer.new(8080)
#   loop do
#     client = server.accept
#     # handle client...
#     client.close
#   end
#   server.close

class TCPServer
  # Create a new TCP server listening on the specified port
  #
  # @param port [Integer] Port number to listen on (1-65535)
  # @param backlog [Integer] Maximum number of pending connections (default: 5)
  # @raise [ArgumentError] if port is invalid
  # @raise [RuntimeError] if server creation fails
  # (Implementation is in C bindings)

  # Accept an incoming client connection (blocking)
  #
  # This method blocks until a client connects to the server.
  # Returns a TCPSocket object representing the client connection.
  #
  # @return [TCPSocket] Connected client socket
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
