# Tests for PicoModem multi-command sessions (issue #425). Each case ends with
# SESSION_QUIT/ABORT or a one-shot command so recv_frame never blocks on EOF.

# In-memory IO. read_nonblock replays a preloaded buffer, write captures output.
class CaptureIO
  def initialize(input = "")
    @input = input.dup
    @pos = 0
    @written = ""
  end

  def written
    @written
  end

  def feed(bytes)
    @input << bytes
    self
  end

  def read(n)
    return nil if @input.bytesize <= @pos
    chunk = @input.byteslice(@pos, n)
    @pos += (chunk ? chunk.bytesize : 0)
    chunk
  end

  def read_nonblock(n)
    read(n)
  end

  def write(s)
    str = s.to_s
    @written << str
    str.bytesize
  end
end

class PicomodemSessionTest < Picotest::Test
  description "PicoModem multi-command sessions"

  # --- helpers -------------------------------------------------------------

  def build_frame(cmd, payload = "")
    body = [cmd].pack("C") + payload
    [0x02, body.bytesize].pack("Cn") + body + [CRC.crc16(body)].pack("n")
  end

  def parse_frames(bytes)
    frames = []
    pos = 0
    while pos < bytes.bytesize
      break unless bytes.getbyte(pos) == 0x02
      len = bytes.byteslice(pos + 1, 2).unpack("n")[0]
      body = bytes.byteslice(pos + 3, len)
      cmd = body.getbyte(0)
      payload = len > 1 ? (body.byteslice(1, len - 1) || "") : ""
      frames << [cmd, payload]
      pos += 3 + len + 2
    end
    frames
  end

  def supported?(bitmap, op)
    (bitmap.getbyte(op >> 3) & (1 << (op & 7))) != 0
  end

  # --- frame encode / parse ------------------------------------------------

  def test_recv_frame_roundtrip
    io = CaptureIO.new(build_frame(PicoModem::FILE_READ, "app.rb"))
    frame = PicoModem.recv_frame(io)
    assert_equal PicoModem::FILE_READ, frame[0]
    assert_equal "app.rb", frame[1]
  end

  # Line noise before a frame is skipped by scanning to the next STX.
  def test_recv_frame_skips_leading_junk
    junk = "\xFF\x00\x41\x7E"
    io = CaptureIO.new(junk + build_frame(PicoModem::FILE_READ, "app.rb"))
    frame = PicoModem.recv_frame(io)
    assert_equal PicoModem::FILE_READ, frame[0]
    assert_equal "app.rb", frame[1]
  end

  def test_send_frame_roundtrip
    io = CaptureIO.new
    PicoModem.send_frame(io, PicoModem::DONE_ACK, [PicoModem::OK].pack("C"))
    frames = parse_frames(io.written)
    assert_equal 1, frames.size
    assert_equal PicoModem::DONE_ACK, frames[0][0]
    assert_equal PicoModem::OK, frames[0][1].getbyte(0)
  end

  # --- session dispatch loop -----------------------------------------------

  # SESSION_OPEN is acknowledged and SESSION_QUIT ends the session gracefully.
  def test_session_open_and_quit
    io = CaptureIO.new(build_frame(PicoModem::SESSION_OPEN) + build_frame(PicoModem::SESSION_QUIT))
    info = PicoModem.run_session(io, io)
    assert_equal "session", info
    frames = parse_frames(io.written)
    assert_equal 2, frames.size
    assert_equal PicoModem::DONE_ACK, frames[0][0] # SESSION_OPEN ack
    assert_equal PicoModem::OK, frames[0][1].getbyte(0)
    assert_equal PicoModem::DONE_ACK, frames[1][0] # SESSION_QUIT ack
  end

  # A recoverable per-command error (unknown command) keeps the session open.
  def test_unknown_command_continues_session
    io = CaptureIO.new(
      build_frame(PicoModem::SESSION_OPEN) +
      build_frame(0x7F) +
      build_frame(PicoModem::SESSION_QUIT)
    )
    info = PicoModem.run_session(io, io)
    assert_equal "session", info
    frames = parse_frames(io.written)
    assert_equal 3, frames.size
    assert_equal PicoModem::DONE_ACK, frames[0][0] # SESSION_OPEN ack
    assert_equal PicoModem::ERROR, frames[1][0]    # unknown command
    assert_equal PicoModem::DONE_ACK, frames[2][0] # SESSION_QUIT ack (session survived)
  end

  # ABORT ends the session immediately, with no DONE ack.
  def test_abort_ends_session
    io = CaptureIO.new(build_frame(PicoModem::SESSION_OPEN) + build_frame(PicoModem::ABORT))
    info = PicoModem.run_session(io, io)
    assert_equal "session", info
    frames = parse_frames(io.written)
    assert_equal 1, frames.size # only the SESSION_OPEN ack
    assert_equal PicoModem::DONE_ACK, frames[0][0]
  end

  # Noise between frames must not count toward SESSION_IDLE_LIMIT.
  def test_session_survives_line_noise
    junk = "\xFF" * (PicoModem::SESSION_IDLE_LIMIT + 4)
    io = CaptureIO.new(
      build_frame(PicoModem::SESSION_OPEN) +
      junk +
      build_frame(PicoModem::SESSION_QUIT)
    )
    info = PicoModem.run_session(io, io)
    assert_equal "session", info
    frames = parse_frames(io.written)
    assert_equal 2, frames.size
    assert_equal PicoModem::DONE_ACK, frames[0][0] # SESSION_OPEN ack
    assert_equal PicoModem::DONE_ACK, frames[1][0] # SESSION_QUIT ack, not a timeout
  end

  # Without a session, one unknown command gets ERROR and the loop returns.
  def test_single_command_mode_stops_after_one
    io = CaptureIO.new(build_frame(0x7F) + build_frame(PicoModem::SESSION_OPEN))
    PicoModem.run_session(io, io)
    frames = parse_frames(io.written)
    assert_equal 1, frames.size # ERROR only. The trailing SESSION_OPEN is never read
    assert_equal PicoModem::ERROR, frames[0][0]
  end

  # A session dispatches many commands in one loop. Missing files make each
  # FILE_READ answer ERROR, proving the loop keeps running until QUIT.
  def test_session_dispatches_multiple_commands
    stub(File).exist? { false }
    io = CaptureIO.new(
      build_frame(PicoModem::SESSION_OPEN) +
      build_frame(PicoModem::FILE_READ, "a.rb") +
      build_frame(PicoModem::FILE_READ, "b.rb") +
      build_frame(PicoModem::SESSION_QUIT)
    )
    PicoModem.run_session(io, io)
    frames = parse_frames(io.written)
    assert_equal 4, frames.size
    assert_equal PicoModem::DONE_ACK, frames[0][0] # SESSION_OPEN ack
    assert_equal PicoModem::ERROR, frames[1][0]    # FILE_READ a.rb (missing)
    assert_equal PicoModem::ERROR, frames[2][0]    # FILE_READ b.rb (missing)
    assert_equal PicoModem::DONE_ACK, frames[3][0] # SESSION_QUIT ack
  end

  # Outside a session a handled command still breaks after one.
  def test_single_command_breaks_after_handler
    stub(File).exist? { false }
    io = CaptureIO.new(
      build_frame(PicoModem::FILE_READ, "a.rb") +
      build_frame(PicoModem::SESSION_QUIT)
    )
    PicoModem.run_session(io, io)
    frames = parse_frames(io.written)
    assert_equal 1, frames.size # ERROR only. Trailing SESSION_QUIT is never read
    assert_equal PicoModem::ERROR, frames[0][0]
  end

  # A stray SESSION_QUIT before any SESSION_OPEN still yields a non-empty status.
  def test_quit_without_open_reports_status
    io = CaptureIO.new(build_frame(PicoModem::SESSION_QUIT))
    info = PicoModem.run_session(io, io)
    assert_equal "quit", info
  end

  # A one-shot ABORT reports a status instead of nil.
  def test_abort_without_session_reports_status
    io = CaptureIO.new(build_frame(PicoModem::ABORT))
    info = PicoModem.run_session(io, io)
    assert_equal "abort", info
  end

  # --- capability check ----------------------------------------------------

  # CMD_CAP replies with the version and a bitmap of the supported opcodes.
  def test_capability_reports_supported_opcodes
    io = CaptureIO.new(build_frame(PicoModem::CMD_CAP))
    PicoModem.run_session(io, io)
    frames = parse_frames(io.written)
    assert_equal 1, frames.size
    assert_equal PicoModem::CMD_CAP_FLAGS, frames[0][0]
    payload = frames[0][1]
    assert_equal PicoModem::PROTOCOL_VERSION, payload.getbyte(0)
    bitmap = payload.byteslice(2, payload.getbyte(1))
    assert_true supported?(bitmap, PicoModem::SESSION_OPEN)
    assert_true supported?(bitmap, PicoModem::FILE_READ)
    assert_false supported?(bitmap, PicoModem::DONE)
  end

  # A capability query works inside a session and does not end it.
  def test_capability_in_session
    io = CaptureIO.new(
      build_frame(PicoModem::SESSION_OPEN) +
      build_frame(PicoModem::CMD_CAP) +
      build_frame(PicoModem::SESSION_QUIT)
    )
    PicoModem.run_session(io, io)
    frames = parse_frames(io.written)
    assert_equal 3, frames.size
    assert_equal PicoModem::DONE_ACK, frames[0][0]      # SESSION_OPEN ack
    assert_equal PicoModem::CMD_CAP_FLAGS, frames[1][0] # capability response
    assert_equal PicoModem::DONE_ACK, frames[2][0]      # SESSION_QUIT ack
  end
end
