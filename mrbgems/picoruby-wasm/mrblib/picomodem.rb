module PicoModem
  # Command types (Host -> Device)
  FILE_READ  = 0x01
  FILE_WRITE = 0x02
  DFU_START  = 0x03
  CHUNK      = 0x04
  DONE       = 0x0F
  ABORT      = 0xFF

  # Response types (Device -> Host) = request | 0x80
  FILE_DATA  = 0x81
  FILE_ACK   = 0x82
  DFU_ACK    = 0x83
  CHUNK_ACK  = 0x84
  DONE_ACK   = 0x8F
  ERROR      = 0xFE

  # Status codes
  OK    = 0x00
  READY = 0x01
  FAIL  = 0xFF

  CHUNK_SIZE = 480
  TIMEOUT_MS = 5000
end
