# picomodem -- the PicoModem (R2P2 Binary Transfer Protocol) client
# for a terminal. Moves one file to or from a device whose shell
# speaks PicoModem (R2P2's shell, Nicht's haush) over its USB CDC
# console, the same exchange the R2P2 web terminal drives.
#
#   picoruby picomodem.rb [-d DEVICE] [-v] put LOCAL [REMOTE]
#   picoruby picomodem.rb [-d DEVICE] [-v] get REMOTE [LOCAL]
#
# DEVICE defaults to /dev/ttyACM0. REMOTE defaults to LOCAL's
# basename, LOCAL (for get) to REMOTE's. The shell must be at its
# prompt: Ctrl-B (STX) is what asks it for the console, and the
# closing "[PicoModem] ..." line it prints is echoed here.
#
# Runs on the host picoruby (mruby VM, mruby-io, mruby-pack). The
# checksums are computed here rather than required, so the script
# needs nothing beyond the default host build.

STX = 0x02
ACK = 0x06

FILE_READ  = 0x01
FILE_WRITE = 0x02
CHUNK      = 0x04
ABORT      = 0xFF

FILE_DATA  = 0x81
FILE_ACK   = 0x82
CHUNK_ACK  = 0x84
DONE_ACK   = 0x8F
ERROR      = 0xFE

OK    = 0x00
READY = 0x01

CHUNK_SIZE = 480
TIMEOUT_MS = 5000

# ---- checksums (picoruby-crc's parameters) ----

# CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection
def crc16(data, crc = 0xFFFF)
  i = 0
  n = data.bytesize
  while i < n
    crc ^= (data.getbyte(i) << 8)
    8.times do
      if (crc & 0x8000) != 0
        crc = ((crc << 1) ^ 0x1021) & 0xFFFF
      else
        crc = (crc << 1) & 0xFFFF
      end
    end
    i += 1
  end
  crc
end

# CRC-32 (zlib): poly 0xEDB88320 reflected, init 0
def crc32(data, crc = 0)
  c = crc ^ 0xFFFFFFFF
  i = 0
  n = data.bytesize
  while i < n
    c ^= data.getbyte(i)
    8.times do
      if (c & 1) != 0
        c = (c >> 1) ^ 0xEDB88320
      else
        c = c >> 1
      end
    end
    i += 1
  end
  c ^ 0xFFFFFFFF
end

# ---- the port ----

class Port
  def initialize(device, verbose)
    @verbose = verbose
    # raw, no echo, and keep DTR up when we close (the device's console
    # is a CDC ACM: the line settings are the tty driver's, not a UART's)
    `stty -F #{device} raw -echo -hupcl 2>&1`
    @io = File.open(device, "r+")
  end

  def close
    @io.close
  end

  def write(bytes)
    @io.syswrite(bytes)
  end

  # up to n bytes, or nil when nothing arrives within timeout_ms.
  # The VM's tick timer interrupts select(2) before its timeout, and
  # mruby-io then answers [[], [], []] at once rather than waiting
  # (core: IO.select does not retry on EINTR), so the deadline is
  # kept here and an empty answer is "not yet", never "ready".
  def read(n, timeout_ms)
    deadline = Time.now.to_f + timeout_ms / 1000.0
    while true
      remaining = deadline - Time.now.to_f
      return nil if remaining <= 0
      ready = IO.select([@io], nil, nil, remaining)
      return nil unless ready
      return @io.sysread(n) unless ready[0].empty?
      sleep_ms 5
    end
  rescue EOFError
    nil
  end

  # exactly n bytes, or nil when the device stops sending
  def read_exact(n, timeout_ms = TIMEOUT_MS)
    buf = ""
    while buf.bytesize < n
      chunk = read(n - buf.bytesize, timeout_ms)
      unless chunk
        log("read_exact timeout need=#{n} got=#{buf.bytesize}")
        return nil
      end
      buf << chunk
    end
    buf
  end

  # drain whatever is pending (prompt echo, a stale line)
  def drain(timeout_ms = 100)
    out = ""
    while (chunk = read(256, timeout_ms))
      out << chunk
    end
    out
  end

  def log(msg)
    STDERR.puts "picomodem: #{msg}" if @verbose
  end
end

# ---- frames ----

def build_frame(cmd, payload = "")
  body = [cmd].pack("C") + payload
  [STX, body.bytesize].pack("Cn") + body + [crc16(body)].pack("n")
end

def send_frame(port, cmd, payload = "")
  port.write(build_frame(cmd, payload))
end

# [cmd, payload] or nil. Bytes before the STX (terminal residue) are
# skipped, as the web terminal does.
def recv_frame(port, timeout_ms = TIMEOUT_MS)
  loop do
    b = port.read_exact(1, timeout_ms)
    return nil unless b
    break if b.getbyte(0) == STX
  end
  len_bytes = port.read_exact(2, timeout_ms)
  return nil unless len_bytes
  length = len_bytes.unpack("n")[0]
  rest = port.read_exact(length + 2, timeout_ms)
  return nil unless rest
  body = rest.byteslice(0, length)
  expected = rest.byteslice(length, 2).unpack("n")[0]
  actual = crc16(body)
  if actual != expected
    port.log("crc16 mismatch expected=#{expected} actual=#{actual}")
    return nil
  end
  [body.getbyte(0), body.byteslice(1, length - 1) || ""]
end

# ---- the session ----

# Ctrl-B, then wait for the shell's ACK
def enter_mode(port)
  port.drain
  port.write([STX].pack("C"))
  waited = 0
  while waited < TIMEOUT_MS
    b = port.read(1, 100)
    if b && b.bytesize > 0
      if b.getbyte(0) == ACK
        # let the device reach its first frame read before sending: over
        # native USB CDC the shell hands the console to the session a beat
        # after the ACK, and a frame that arrives first is read at the
        # prompt, not by the session
        sleep_ms 200
        return true
      end
      next
    end
    waited += 100
  end
  false
end

# the shell prints "[PicoModem] ..." 200ms after the session; show it
def leave_mode(port, abort)
  if abort
    send_frame(port, ABORT)
  end
  sleep 0.4
  tail = port.drain(200)
  tail.each_line do |line|
    line = line.strip
    puts line if line.start_with?("[PicoModem]")
  end
end

# abort: whether the device is still inside the transfer (an ERROR or
# DONE_ACK from it already ended the session, and an ABORT frame sent
# after that would land on the shell's prompt as keystrokes)
def fail_with(port, message, abort = true)
  STDERR.puts "picomodem: #{message}"
  leave_mode(port, abort)
  port.close
  exit 1
end

def error_text(payload)
  payload.to_s
end

def do_put(port, local, remote)
  data = File.open(local, "rb") { |f| f.read }
  unless data
    STDERR.puts "picomodem: cannot read #{local}"
    exit 1
  end
  fail_with(port, "no ACK from the shell (is it at the prompt?)", false) unless enter_mode(port)
  send_frame(port, FILE_WRITE, [data.bytesize].pack("N") + remote)
  frame = recv_frame(port)
  fail_with(port, "timeout waiting for FILE_ACK") unless frame
  fail_with(port, "device: #{error_text(frame[1])}", false) if frame[0] == ERROR
  fail_with(port, "unexpected response 0x#{frame[0].to_s(16)}") unless frame[0] == FILE_ACK
  offset = 0
  while offset < data.bytesize
    size = data.bytesize - offset
    size = CHUNK_SIZE if CHUNK_SIZE < size
    send_frame(port, CHUNK, data.byteslice(offset, size))
    ack = recv_frame(port)
    fail_with(port, "timeout waiting for CHUNK_ACK at #{offset}") unless ack
    fail_with(port, "device: #{error_text(ack[1])}", false) if ack[0] == ERROR
    fail_with(port, "unexpected response 0x#{ack[0].to_s(16)}") unless ack[0] == CHUNK_ACK
    offset += size
    port.log("sent #{offset}/#{data.bytesize}")
  end
  done = recv_frame(port)
  fail_with(port, "timeout waiting for DONE_ACK") unless done
  fail_with(port, "device: #{error_text(done[1])}", false) if done[0] == ERROR
  fail_with(port, "unexpected response 0x#{done[0].to_s(16)}") unless done[0] == DONE_ACK
  status, remote_crc = done[1].byteslice(0, 5).unpack("CN")
  local_crc = crc32(data)
  fail_with(port, "device reported status #{status}", false) unless status == OK
  if local_crc != remote_crc
    fail_with(port, "CRC32 mismatch local=0x#{local_crc.to_s(16)} remote=0x#{remote_crc.to_s(16)}", false)
  end
  puts "put #{local} -> #{remote} (#{data.bytesize} bytes, crc32 0x#{local_crc.to_s(16)})"
  leave_mode(port, false)
end

def do_get(port, remote, local)
  fail_with(port, "no ACK from the shell (is it at the prompt?)", false) unless enter_mode(port)
  send_frame(port, FILE_READ, remote)
  data = ""
  total = nil
  loop do
    frame = recv_frame(port)
    fail_with(port, "timeout waiting for FILE_DATA") unless frame
    cmd = frame[0]
    payload = frame[1]
    if cmd == FILE_DATA
      if total.nil?
        fail_with(port, "FILE_DATA without a size header") if payload.bytesize < 4
        total = payload.unpack("N")[0]
        payload = payload.byteslice(4, payload.bytesize - 4) || ""
      end
      data << payload
      port.log("received #{data.bytesize}/#{total}")
      send_frame(port, CHUNK_ACK, [OK].pack("C"))
    elsif cmd == DONE_ACK
      status, remote_crc = payload.byteslice(0, 5).unpack("CN")
      fail_with(port, "device reported status #{status}", false) unless status == OK
      local_crc = crc32(data)
      if local_crc != remote_crc
        fail_with(port, "CRC32 mismatch local=0x#{local_crc.to_s(16)} remote=0x#{remote_crc.to_s(16)}", false)
      end
      break
    elsif cmd == ERROR
      fail_with(port, "device: #{error_text(payload)}", false)
    else
      fail_with(port, "unexpected response 0x#{cmd.to_s(16)}")
    end
  end
  File.open(local, "wb") { |f| f.write(data) }
  puts "get #{remote} -> #{local} (#{data.bytesize} bytes, crc32 0x#{crc32(data).to_s(16)})"
  leave_mode(port, false)
end

# ---- the command line ----

def usage
  STDERR.puts "usage: picomodem [-d DEVICE] [-v] put LOCAL [REMOTE]"
  STDERR.puts "       picomodem [-d DEVICE] [-v] get REMOTE [LOCAL]"
  STDERR.puts "DEVICE defaults to /dev/ttyACM0"
  exit 2
end

def basename(path)
  i = path.rindex("/")
  i ? path[(i + 1)..-1] : path
end

device = "/dev/ttyACM0"
verbose = false
args = []
argv = ARGV.dup
while (a = argv.shift)
  case a
  when "-d", "--device"
    device = argv.shift or usage
  when "-v", "--verbose"
    verbose = true
  when "-h", "--help"
    usage
  else
    args << a
  end
end

usage if args.size < 2 || args.size > 3
op = args[0]
usage unless op == "put" || op == "get"

port = Port.new(device, verbose)
if op == "put"
  local = args[1]
  remote = args[2] || basename(local)
  do_put(port, local, remote)
else
  remote = args[1]
  local = args[2] || basename(remote)
  do_get(port, remote, local)
end
port.close
