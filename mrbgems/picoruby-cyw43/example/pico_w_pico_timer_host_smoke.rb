require "json"

class SocketError < StandardError
end

module Kernel
  def sleep_ms(ms)
    Machine.advance(ms)
  end
end

module Machine
  @board_millis = 0

  class << self
    attr_accessor :board_millis

    def advance(ms)
      @board_millis += ms
    end
  end
end

module CYW43
  @ap_active = false
  @ap_ssid = nil
  @ap_ip = "192.168.4.1"
  @ap_station_count = 1
  @ap_max_stations = 10

  class GPIO
    LED_PIN = 0

    def initialize(_pin)
      @value = 0
    end

    def write(value)
      @value = value
    end

    def high?
      @value == 1
    end
  end

  class << self
    def init(_country)
      true
    end

    def enable_ap_mode(ssid, _password)
      @ap_active = true
      @ap_ssid = ssid
      true
    end

    def ap_active?
      @ap_active
    end

    def ap_ssid
      @ap_ssid
    end

    def ap_ipv4_address
      @ap_ip
    end

    def ap_station_count
      @ap_station_count
    end

    def ap_max_stations
      @ap_max_stations
    end
  end
end

class GPIO
  OUT = :out

  def initialize(_pin, _mode = nil)
    @value = 0
  end

  def write(value)
    @value = value
  end

  def read
    @value
  end
end

class TCPServer
  def initialize(*)
  end

  def accept_nonblock
    nil
  end
end

class FakeClient
  attr_reader :written

  def initialize(request_line, headers: [], body: "")
    @lines = [request_line, *headers, "\r\n"]
    @body = body.dup
    @written = +""
  end

  def gets
    @lines.shift
  end

  def readpartial(maxlen)
    raise EOFError if @body.empty?

    chunk = @body.byteslice(0, maxlen)
    @body = @body.byteslice(chunk.bytesize..) || +""
    chunk
  end

  def read_nonblock(maxlen)
    if @lines.any?
      @lines.shift
    elsif !@body.empty?
      readpartial(maxlen)
    end
  end

  def closed?
    @lines.empty? && @body.empty?
  end

  def write(data)
    text = data.to_s
    @written << text
    text.bytesize
  end

  def close
  end
end

source_path = File.expand_path("pico_w_pico_timer.rb", __dir__)
source = File.read(source_path)
source = source.lines.reject { |line| line.start_with?("require ") }.join
source = source.sub(/\nPicoTimerApp\.boot\nPicoTimerApp\.serve\n?\z/, "\n")
TOPLEVEL_BINDING.eval(source, source_path)

PicoTimerApp.boot

root_client = FakeClient.new("GET / HTTP/1.1\r\n")
PicoTimerApp.handle_client(root_client)
raise "root route failed" unless root_client.written.include?("200 OK")
raise "root page missing title" unless root_client.written.include?("PICO TIMER")

status_client = FakeClient.new("GET /status HTTP/1.1\r\n")
PicoTimerApp.handle_client(status_client)
raise "status route failed" unless status_client.written.include?("200 OK")
raise "status payload missing" unless status_client.written.include?("\"running\":false")

set_client = FakeClient.new("GET /set?sec=10 HTTP/1.1\r\n")
PicoTimerApp.handle_client(set_client)
raise "set route did not render action page" unless set_client.written.include?("200 OK")
raise "set route missing message" unless set_client.written.include?("Timer started")

payload = PicoTimerApp.status_payload
raise "timer did not start" unless payload["running"]
raise "remaining not set" unless payload["remaining"] > 0

Machine.advance(11_000)
PicoTimerApp.update_timer
payload = PicoTimerApp.status_payload
raise "timer did not stop after expiry" if payload["running"]
raise "alert led did not turn on" unless payload["led"] == 1

ledoff_client = FakeClient.new("GET /ledoff HTTP/1.1\r\n")
PicoTimerApp.handle_client(ledoff_client)
raise "ledoff route did not render action page" unless ledoff_client.written.include?("200 OK")
raise "alert led did not turn off" unless PicoTimerApp.status_payload["led"] == 0

stop_client = FakeClient.new("GET /stop HTTP/1.1\r\n")
PicoTimerApp.handle_client(stop_client)
raise "stop route did not render action page" unless stop_client.written.include?("200 OK")

puts "Host smoke test passed"
