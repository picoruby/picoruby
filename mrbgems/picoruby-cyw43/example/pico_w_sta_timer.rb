require "cyw43"
require "gpio"
require "machine"
require "socket"

# Minimal STA timer baseline.
# Chrome-first, no JS/fetch/polling, GET-only control path.

module PicoStaTimerApp
  STA_SSID = "U3D_0QJQK9"
  STA_PASSWORD = "48585901"
  ALERT_LED_PIN = 15
  DEFAULT_SECONDS = 10
  MAX_SECONDS = 60 * 60
  LOOP_SLEEP_MS = 10
  HEARTBEAT_INTERVAL_MS = 1000
  REQUEST_TIMEOUT_MS = 1500
  RESPONSE_CHUNK_SIZE = 512

  class << self
    def boot
      puts "STA starting"
      puts "Connecting to Wi-Fi: #{STA_SSID}"

      CYW43.init("JP")
      CYW43.enable_sta_mode

      connect_timeout_result = nil
      2.times do |i|
        attempt = i + 1
        begin
          connect_timeout_result = CYW43.connect_timeout(STA_SSID, STA_PASSWORD, CYW43::Auth::WPA2_MIXED_PSK, 30)
          break
        rescue => e
          puts "STA connect_timeout attempt=#{attempt} exception: #{e.class}: #{e.message}"
          if attempt == 1
            sleep_ms 100
          end
        end
      end
      unless connect_timeout_result == true
        puts "STA connect_timeout failed twice; aborting server start"
        return false
      end

      ip = wait_for_ip_attempt(1)
      if !ip || ip.to_s.empty?
        ip = wait_for_ip_attempt(2)
      end
      unless ip && !ip.to_s.empty?
        puts "STA IP unassigned; aborting server start"
        return false
      end
      puts "STA connected"
      puts "STA IP: #{ip}"
      puts "Open: http://#{ip}/"

      @heartbeat_led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
      @alert_led = build_alert_led
      @alert_led.write(0) if @alert_led

      @last_heartbeat_ms = now_ms
      @running = false
      @deadline_ms = nil
      @server = TCPServer.new(nil, 80)

      puts "HTTP server listening on port 80"
      true
    rescue => e
      puts "STA boot error: #{e.class}: #{e.message}"
      false
    end

    def serve
      loop do
        client = nil

        begin
          heartbeat
          update_timer

          client = @server.accept_nonblock
          unless client
            sleep_ms LOOP_SLEEP_MS
            next
          end

          handle_client(client)
        rescue => e
          puts "STA timer serve loop error: #{e.class}: #{e.message}"
          safe_error_response(client)
        ensure
          client.close if client
        end
      end
    end

    def handle_client(client)
      request_line = read_http_request(client)
      return unless request_line

      method, target = parse_request_line(request_line)
      path = parse_path(target)

      if method == "GET" && %w[/set /start /stop /ledoff].include?(path)
        puts "Request: #{request_line.inspect}"
      end

      route_request(client, method, path, target)
    rescue => e
      puts "STA timer error: #{e.class}: #{e.message}"
      safe_error_response(client)
    end

    def route_request(client, method, path, target)
      case [method, path]
      when ["GET", "/"]
        write_response(client, "200 OK", "text/html; charset=utf-8", html_page)
      when ["GET", "/status"]
        write_response(client, "200 OK", "application/json", status_json)
      when ["GET", "/set"], ["GET", "/start"]
        set_timer(query_value(target, "sec") || query_value(target, "seconds"))
        write_response(client, "200 OK", "text/html; charset=utf-8", action_page("Timer started"))
      when ["GET", "/stop"]
        stop_timer
        write_response(client, "200 OK", "text/html; charset=utf-8", action_page("Timer stopped"))
      when ["GET", "/ledoff"]
        turn_alert_led_off
        write_response(client, "200 OK", "text/html; charset=utf-8", action_page("Alert LED turned off"))
      else
        write_response(client, "404 Not Found", "text/plain", "Not Found\n")
      end
    end

    def set_timer(seconds_value)
      seconds = seconds_value ? seconds_value.to_i : DEFAULT_SECONDS
      seconds = DEFAULT_SECONDS if seconds <= 0
      seconds = MAX_SECONDS if seconds > MAX_SECONDS

      @deadline_ms = now_ms + (seconds * 1000)
      @running = true
      turn_alert_led_off
    end

    def stop_timer
      @running = false
      @deadline_ms = nil
    end

    def turn_alert_led_off
      @alert_led.write(0) if @alert_led
    end

    def update_timer
      return unless @running
      return unless @deadline_ms

      return unless @deadline_ms <= now_ms

      @running = false
      @deadline_ms = nil
      @alert_led.write(1) if @alert_led
    end

    def remaining_seconds
      return 0 unless @running
      return 0 unless @deadline_ms

      diff = @deadline_ms - now_ms
      return 0 if diff <= 0

      (diff + 999) / 1000
    end

    def status_json
      ip = CYW43.ipv4_address || ""
      led = @alert_led ? @alert_led.read : 0

      "{\"running\":#{@running ? 'true' : 'false'}," \
      "\"remaining\":#{remaining_seconds}," \
      "\"led\":#{led}," \
      "\"ssid\":\"#{json_escape(STA_SSID)}\"," \
      "\"ip\":\"#{json_escape(ip)}\"}"
    end

    def html_page
      ip = CYW43.ipv4_address || "unassigned"
      led = @alert_led ? @alert_led.read : 0

      body = +""
      body << "<!doctype html><html><head><meta charset=\"utf-8\">"
      body << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      body << "<title>PICO TIMER</title></head><body>"
      body << "<h1>PICO TIMER</h1>"
      body << "<p>SSID: #{STA_SSID}</p>"
      body << "<p>IP: #{ip}</p>"
      body << "<p>Running: #{@running}</p>"
      body << "<p>Remaining: #{remaining_seconds} sec</p>"
      body << "<p>LED: #{led == 1 ? "on" : "off"}</p>"
      body << "<form action=\"/set\" method=\"get\">"
      body << "<label>Seconds: <input name=\"sec\" type=\"number\" min=\"1\" max=\"#{MAX_SECONDS}\" value=\"#{DEFAULT_SECONDS}\"></label>"
      body << "<button type=\"submit\">Start 10s</button></form>"
      body << "<form action=\"/stop\" method=\"get\"><button type=\"submit\">Stop</button></form>"
      body << "<form action=\"/ledoff\" method=\"get\"><button type=\"submit\">LED Off</button></form>"
      body << "<p><a href=\"/\">Refresh</a></p>"
      body << "<p><a href=\"/status\">View JSON status</a></p>"
      body << "</body></html>"
      body
    end

    def action_page(message)
      ip = CYW43.ipv4_address || "unassigned"
      led = @alert_led ? @alert_led.read : 0

      body = +""
      body << "<!doctype html><html><head><meta charset=\"utf-8\">"
      body << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      body << "<title>PICO TIMER</title></head><body>"
      body << "<h1>PICO TIMER</h1>"
      body << "<p>#{message}</p>"
      body << "<p>IP: #{ip}</p>"
      body << "<p>Running: #{@running}</p>"
      body << "<p>Remaining: #{remaining_seconds} sec</p>"
      body << "<p>LED: #{led == 1 ? "on" : "off"}</p>"
      body << "<p><a href=\"/\">Back</a></p>"
      body << "<p><a href=\"/status\">View JSON status</a></p>"
      body << "<p><a href=\"/\">Refresh</a></p>"
      body << "</body></html>"
      body
    end

    def heartbeat
      return unless @heartbeat_led
      return unless now_ms - @last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS

      @heartbeat_led.write(@heartbeat_led.high? ? 0 : 1)
      @last_heartbeat_ms = now_ms
    end

    def build_alert_led
      GPIO.new(ALERT_LED_PIN, GPIO::OUT)
    rescue => e
      puts "External LED unavailable on GP#{ALERT_LED_PIN}: #{e.message}"
      nil
    end

    def wait_for_ip
      30.times do
        ip = CYW43.ipv4_address
        return ip if ip
        sleep_ms 100
      end

      CYW43.ipv4_address
    end

    def wait_for_ip_attempt(attempt)
      wait_for_ip
    end

    def now_ms
      Machine.board_millis
    end

    def sleep_ms(ms)
      Machine.delay_ms(ms)
    end

    def parse_request_line(request_line)
      parts = request_line.to_s.split(" ")
      [parts[0] || "GET", parts[1] || "/"]
    end

    def parse_path(target)
      target.to_s.split("?", 2)[0] || "/"
    end

    def query_value(target, key)
      query = target.to_s.split("?", 2)[1]
      return nil unless query

      query.split("&").each do |pair|
        k, v = pair.split("=", 2)
        next unless k == key

        return url_decode(v.to_s)
      end

      nil
    end

    def read_http_request(client)
      buffer = +""
      deadline_ms = now_ms + REQUEST_TIMEOUT_MS

      while now_ms < deadline_ms
        chunk = client.read_nonblock(512)
        buffer << chunk if chunk

        if buffer.include?("\r\n\r\n") || buffer.include?("\n\n")
          return buffer.split("\r\n", 2).first || buffer.split("\n", 2).first
        end

        return nil if client.closed? && buffer.empty?

        sleep_ms LOOP_SLEEP_MS
      end

      puts "HTTP request timeout"
      nil
    rescue EOFError, SocketError => e
      puts "HTTP request read failed: #{e.class}: #{e.message}"
      nil
    end

    def url_decode(value)
      source = value.to_s
      decoded = +""
      i = 0

      while i < source.bytesize
        ch = source.byteslice(i, 1)

        if ch == "+"
          decoded << " "
          i += 1
          next
        end

        if ch == "%" && i + 2 < source.bytesize
          hex = source.byteslice(i + 1, 2)
          if hex_digit?(hex.byteslice(0, 1)) && hex_digit?(hex.byteslice(1, 1))
            decoded << hex.hex.chr
            i += 3
            next
          end
        end

        decoded << ch
        i += 1
      end

      decoded
    end

    def hex_digit?(char)
      return false unless char

      ("0" <= char && char <= "9") ||
        ("A" <= char && char <= "F") ||
        ("a" <= char && char <= "f")
    end

    def write_response(client, status, content_type, body)
      client.write "HTTP/1.1 #{status}\r\n"
      client.write "Content-Type: #{content_type}\r\n"
      client.write "Content-Length: #{body.bytesize}\r\n"
      client.write "Cache-Control: no-store\r\n"
      client.write "Connection: close\r\n"
      client.write "\r\n"

      offset = 0
      while offset < body.bytesize
        chunk = body.byteslice(offset, RESPONSE_CHUNK_SIZE)
        break unless chunk

        client.write chunk
        offset += chunk.bytesize
      end
    end

    def safe_error_response(client)
      return unless client

      begin
        write_response(client, "500 Internal Server Error", "text/plain", "Internal Server Error\n")
      rescue => e
        puts "STA timer error response failed: #{e.class}: #{e.message}"
      end
    end

    def json_escape(text)
      text.to_s.gsub("\\", "\\\\").gsub('"', '\"')
    end
  end
end

if PicoStaTimerApp.boot
  PicoStaTimerApp.serve
end
