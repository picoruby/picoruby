#
# WebSocket library for PicoRuby
# This is a pure Ruby implementation of WebSocket (RFC 6455).
# Designed for real-time communication in IoT devices.
#

module Net
  module WebSocket
    class WebSocketError < StandardError; end
    class HandshakeError < WebSocketError; end
    class ProtocolError < WebSocketError; end
    class ConnectionClosed < WebSocketError; end

    # Opcodes
    OPCODE_CONTINUATION = 0x0
    OPCODE_TEXT = 0x1
    OPCODE_BINARY = 0x2
    OPCODE_CLOSE = 0x8
    OPCODE_PING = 0x9
    OPCODE_PONG = 0xA

    # Close codes
    CLOSE_NORMAL = 1000
    CLOSE_GOING_AWAY = 1001
    CLOSE_PROTOCOL_ERROR = 1002
    CLOSE_UNSUPPORTED_DATA = 1003
    CLOSE_ABNORMAL = 1006

    # Magic GUID for handshake
    WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
  end
end
