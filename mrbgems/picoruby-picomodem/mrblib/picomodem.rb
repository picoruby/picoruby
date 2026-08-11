# PicoModem - R2P2 Binary Transfer Protocol
# Machine-to-machine communication protocol for file transfer
# Initiated by STX (0x02) from the host side
#
# Frame format:
#   STX (0x02)   1 byte   - frame start marker
#   Length        2 bytes  - big-endian, size of Cmd + Payload
#   Cmd           1 byte   - command type
#   Payload       N bytes  - command-specific data
#   CRC16         2 bytes  - big-endian, CRC-16/CCITT over Cmd + Payload

require 'crc'
require 'pack'

module PicoModem

  # Command types (Host -> Device)
  FILE_READ    = 0x01
  FILE_WRITE   = 0x02
  DFU_START    = 0x03
  CHUNK        = 0x04
  SESSION_QUIT = 0x0D
  SESSION_OPEN = 0x0E
  DONE         = 0x0F
  ABORT        = 0xFF

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
  # Consecutive failed reads (timeout or bad frame) that end an idle session.
  SESSION_IDLE_LIMIT = 12

  # Run a PicoModem session from the shell: raw terminal, dispatch, then restore.
  def self.session(io_in, io_out)
    STDIN.raw!
    info = nil
    begin
      info = run_session(io_in, io_out)
    ensure
      STDIN.cooked! # restore the terminal even if run_session raises
    end
    # Wait for browser to stop binary capture before printing
    sleep_ms 200
    puts "[PicoModem] #{info}"
  end

  # Read and dispatch frames. One command per call, or - after SESSION_OPEN - a
  # loop until SESSION_QUIT/ABORT/idle timeout. Returns the shell status string.
  def self.run_session(io_in, io_out)
    info = nil
    in_session = false
    idle = 0
    begin
      while true
        frame = recv_frame(io_in)
        unless frame
          if in_session
            idle += 1
            next if idle < SESSION_IDLE_LIMIT
            info = "session timeout"
          else
            info = "timeout"
          end
          break
        end
        idle = 0
        cmd = frame[0]
        payload = frame[1]
        case cmd
        when SESSION_OPEN
          info = "session"
          in_session = true
          send_frame(io_out, DONE_ACK, [OK].pack("C"))
        when SESSION_QUIT
          info ||= "quit"
          send_frame(io_out, DONE_ACK, [OK].pack("C"))
          break
        when FILE_READ
          info = "read #{payload}"
          handle_file_read(io_in, io_out, payload)
          break unless in_session
        when FILE_WRITE
          path = 4 < payload.bytesize ? payload.byteslice(4, payload.bytesize - 4) : ""
          info = "write #{path}"
          handle_file_write(io_in, io_out, payload)
          break unless in_session
        when DFU_START
          info = "dfu"
          handle_dfu(io_in, io_out, payload)
          break
        when ABORT
          info ||= "abort"
          break
        else
          send_frame(io_out, ERROR, "Unknown command")
          break unless in_session
        end
      end
    rescue => e
      info = "error: #{e.message}"
      begin
        send_frame(io_out, ERROR, e.message.to_s)
      rescue
        # best effort
      end
    end
    info
  end

  # Receive one PicoModem frame from io
  # Returns [cmd, payload] or nil on timeout/error
  def self.recv_frame(io)
    # Read STX
    stx = read_exact(io, 1)
    return nil unless stx
    unless stx.getbyte(0) == 0x02
      return nil
    end
    # Read length (2 bytes big-endian)
    len_bytes = read_exact(io, 2)
    return nil unless len_bytes
    # @type var length: Integer
    length = len_bytes.unpack("n")[0]
    # Read cmd + payload + CRC16
    rest = read_exact(io, length + 2)
    return nil unless rest
    body = rest.byteslice(0, length)
    return nil unless body
    expected_crc = (rest.byteslice(length, 2) || '').unpack("n")[0]
    actual_crc = CRC.crc16(body)
    unless actual_crc == expected_crc
      return nil
    end
    cmd = body.getbyte(0) || 0
    payload = 1 < length ? (body&.byteslice(1, length - 1) || '') : ""
    [cmd, payload]
  end

  # Send one PicoModem frame
  def self.send_frame(io, cmd, payload = "")
    body = [cmd].pack("C") + payload.to_s
    length = body.bytesize
    crc = CRC.crc16(body)
    frame = [0x02, length, crc].pack("Cnn")
    frame = (frame.byteslice(0, 3) || '') + body + (frame.byteslice(3, 2) || '')
    io.write(frame)
  end

  # Read exactly n bytes from io with timeout.
  # Uses elapsed-time counting instead of absolute timestamps
  # to avoid 32-bit integer overflow on embedded targets.
  def self.read_exact(io, n)
    buf = ""
    waited = 0
    while buf.bytesize < n
      chunk = io.read_nonblock(n - buf.bytesize)
      if chunk
        buf << chunk
        waited = 0
      else
        if TIMEOUT_MS <= waited
          return nil
        end
        sleep_ms 1
        waited += 1
      end
    end
    buf
  end

  # Handle FILE_READ: send file contents in chunks.
  # Reads the file in chunks to avoid loading the entire file into RAM.
  # CRC32 is computed incrementally across chunks.
  def self.handle_file_read(io_in, io_out, payload)
    path = payload.to_s
    unless File.exist?(path)
      send_frame(io_out, ERROR, "File not found: #{path}")
      return
    end
    total = File::Stat.new(path).size
    file_crc = 0
    File.open(path, "r") do |f|
      first = true
      while true
        chunk = f.read(CHUNK_SIZE)
        break unless chunk && 0 < chunk.bytesize
        file_crc = CRC.crc32(chunk, file_crc)
        frame_payload = ""
        if first
          frame_payload << [total].pack("N")
          first = false
        end
        frame_payload << chunk
        send_frame(io_out, FILE_DATA, frame_payload)
        ack = recv_frame(io_in)
        unless ack && ack[0] == CHUNK_ACK
          # No CHUNK_ACK: emit a terminal ERROR so a session sees a clean boundary.
          send_frame(io_out, ERROR, "FILE_READ aborted: missing CHUNK_ACK")
          return
        end
      end
    end
    send_frame(io_out, DONE_ACK, [OK, file_crc].pack("CN"))
  end

  # Handle FILE_WRITE: receive file contents in chunks and stream them to flash
  def self.handle_file_write(io_in, io_out, payload)
    # Payload: 4 bytes total size (big-endian) + path
    if payload.bytesize < 5
      send_frame(io_out, ERROR, "Invalid FILE_WRITE payload")
      return
    end
    # @type var total: Integer
    total = (payload.byteslice(0, 4) || '').unpack("N")[0]
    path = payload.byteslice(4, payload.bytesize - 4)
    raise "Invalid file path" if path.nil? || path.empty?
    send_frame(io_out, FILE_ACK, [READY].pack("C"))

    written = 0
    file_crc = 0
    aborted = false
    error_message = nil

    File.unlink(path) if File.exist?(path)
    begin
      File.open(path, "w") do |file|
        file.expand(total) if file.respond_to?(:expand)

        while written < total
          frame = recv_frame(io_in)
          unless frame
            error_message = "Timeout receiving chunk"
            break
          end
          case frame[0]
          when CHUNK
            chunk = frame[1]
            file.write(chunk)
            file_crc = CRC.crc32(chunk, file_crc)
            written += chunk.bytesize
            send_frame(io_out, CHUNK_ACK, [OK].pack("C"))
          when ABORT
            aborted = true
            break
          else
            error_message = "Unexpected command during transfer"
            break
          end
        end
      end
    rescue => e
      error_message = e.message
    end

    if error_message
      File.unlink(path) if File.exist?(path)
      send_frame(io_out, ERROR, error_message.to_s)
    elsif aborted
      File.unlink(path) if File.exist?(path)
    else
      send_frame(io_out, DONE_ACK, [OK, file_crc].pack("CN"))
    end
  end

  # Handle DFU_START: receive firmware via PicoModem and feed to DFU::Updater
  def self.handle_dfu(io_in, io_out, payload)
    # Payload contains the DFU header (19+ bytes with optional signature)
    expected_total = DFU::Updater.expected_size(payload)
    unless expected_total
      send_frame(io_out, ERROR, "Invalid DFU header")
      return
    end
    body_size = expected_total - payload.bytesize
    send_frame(io_out, DFU_ACK, [READY].pack("C"))
    # Receive firmware body chunks (size known from DFU header)
    data = ""
    while data.bytesize < body_size
      frame = recv_frame(io_in)
      unless frame
        send_frame(io_out, ERROR, "Timeout receiving DFU data")
        return
      end
      case frame[0]
      when CHUNK
        data << frame[1]
        send_frame(io_out, CHUNK_ACK, [OK].pack("C"))
      when ABORT
        return
      else
        send_frame(io_out, ERROR, "Unexpected command during DFU")
        return
      end
    end
    # Feed complete DFU packet to updater via in-memory reader.
    # Suppress puts output from updater to avoid corrupting PicoModem frames.
    dfu_packet = payload + data
    reader = PicoModemStringReader.new(dfu_packet)
    old_stdout = $stdout
    begin
      $stdout = PicoModemNullIO.new
      updater = DFU::Updater.new
      updater.receive(reader)
      $stdout = old_stdout
      send_frame(io_out, DONE_ACK, [OK].pack("C"))
    rescue => e
      $stdout = old_stdout
      send_frame(io_out, ERROR, e.message)
    end
  end

end

# IO-like reader that reads from an in-memory String buffer.
# Used to feed pre-received DFU packets to DFU::Updater#receive.
class PicoModemStringReader
  def initialize(data)
    @data = data
    @pos = 0
  end

  def read(n)
    return nil if @data.bytesize <= @pos
    chunk = @data.byteslice(@pos, n)
    @pos += (chunk&.bytesize || 0)
    chunk
  end

  def read_nonblock(n)
    read(n)
  end
end

# Null IO that discards all output.
# Used to suppress DFU::Updater log messages during PicoModem session.
class PicoModemNullIO
  def write(*_args)
    nil
  end

  def puts(*_args)
    nil
  end

  def print(*_args)
    nil
  end
end
