# Backward compatibility: load TCP server implementation
# This file exists for compatibility with existing code that requires 'drb_server'
# The actual implementation is in drb_tcp_server.rb

# Note: This will only work if socket is available
# For socket-less environments, use protocol-specific servers directly
