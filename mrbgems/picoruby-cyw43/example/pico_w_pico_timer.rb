require "cyw43"
require "gpio"
require "json"
require "machine"
require "socket"

module PicoTimerApp
  AP_SSID = "PICO-TIMER"
  AP_PASSWORD = "12345678"
  ALERT_LED_PIN = 15
  DEFAULT_SECONDS = 10
  MAX_SECONDS = 60 * 60
  LOOP_SLEEP_MS = 10
  HEARTBEAT_INTERVAL_MS = 1000
  AP_ENSURE_INTERVAL_MS = 5000
  RESPONSE_CHUNK_SIZE = 512

  class << self
    def boot
      CYW43.init("JP")
      CYW43.enable_ap_mode(AP_SSID, AP_PASSWORD)

      @heartbeat_led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
      @alert_led = build_alert_led
      @alert_led.write(0) if @alert_led

      @last_heartbeat_ms = now_ms
      @last_ap_ensure_ms = now_ms
      @running = false
      @deadline_ms = nil
      @server = TCPServer.new(nil, 80)

      puts "AP started"
      puts "AP active?: #{CYW43.ap_active?}"
      puts "AP SSID: #{CYW43.ap_ssid || 'unknown'}"
      puts "AP IP: #{CYW43.ap_ipv4_address || 'unassigned'}"
      puts "AP stations: #{CYW43.ap_station_count}/#{CYW43.ap_max_stations}"
      puts "Open: http://#{CYW43.ap_ipv4_address}/"
    end

    def serve
      loop do
        client = nil

        begin
          heartbeat
          update_timer
          ensure_ap

          client = @server.accept_nonblock
          unless client
            sleep_ms LOOP_SLEEP_MS
            next
          end

          handle_client(client)
        rescue => e
          puts "PICO TIMER serve loop error: #{e.class}: #{e.message}"
          safe_error_response(client)
        ensure
          client.close if client
        end
      end
    end

    def handle_client(client)
      request_line, headers, body = read_http_request(client)
      return unless request_line

      method, target = parse_request_line(request_line)
      path, query_params = parse_request_target(target)
      body_params = parse_www_form(body)
      params = query_params.merge(body_params)

      puts "Request: #{request_line.inspect}"
      route_request(client, method, path, params)
    rescue => e
      puts "PICO TIMER error: #{e.class}: #{e.message}"
      safe_error_response(client)
    end

    def route_request(client, method, path, params)
      case [method, path]
      when ["GET", "/"]
        write_response(client, "200 OK", "text/html; charset=utf-8", html_page)
      when ["GET", "/status"]
        write_json(client, status_payload)
      when ["GET", "/set"], ["POST", "/set"], ["GET", "/start"]
        set_timer(params["sec"] || params["seconds"])
        write_action_response(client, method, "Timer started")
      when ["GET", "/stop"], ["POST", "/stop"]
        stop_timer
        write_action_response(client, method, "Timer stopped")
      when ["GET", "/ledoff"], ["POST", "/ledoff"]
        turn_alert_led_off
        write_action_response(client, method, "Alert LED turned off")
      else
        write_response(client, "404 Not Found", "text/plain", "Not Found\n")
      end
    end

    def set_timer(seconds_value)
      seconds = seconds_value ? seconds_value.to_i : DEFAULT_SECONDS
      seconds = 1 if seconds <= 0
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

      if @deadline_ms <= now_ms
        @running = false
        @deadline_ms = nil
        @alert_led.write(1) if @alert_led
      end
    end

    def remaining_seconds
      return 0 unless @running
      return 0 unless @deadline_ms

      diff = @deadline_ms - now_ms
      return 0 if diff <= 0

      (diff + 999) / 1000
    end

    def status_payload
      {
        "running" => @running,
        "remaining" => remaining_seconds,
        "led" => @alert_led ? @alert_led.read : 0,
        "ssid" => CYW43.ap_ssid,
        "ap_ip" => CYW43.ap_ipv4_address,
        "stations" => CYW43.ap_station_count,
        "stations_max" => CYW43.ap_max_stations
      }
    end

    def html_page
      state = status_payload

      <<~HTML
        <!doctype html>
        <html>
        <head>
          <meta charset="utf-8">
          <meta name="viewport" content="width=device-width,initial-scale=1">
          <title>PICO TIMER</title>
        </head>
        <body>
          <h1>PICO TIMER</h1>
          <p>SSID: #{state["ssid"]}</p>
          <p>AP IP: #{state["ap_ip"]}</p>
          <p>Stations: #{state["stations"]}/#{state["stations_max"]}</p>
          <p>Running: #{state["running"]}</p>
          <p>Remaining: #{state["remaining"]} sec</p>
          <p>Alert LED: #{state["led"] == 1 ? "on" : "off"}</p>

          <form action="/set" method="get">
            <label>
              Seconds:
              <input name="sec" type="number" min="1" max="#{MAX_SECONDS}" value="#{DEFAULT_SECONDS}">
            </label>
            <button type="submit">Start</button>
          </form>

          <form action="/stop" method="get">
            <button type="submit">Stop</button>
          </form>

          <form action="/ledoff" method="get">
            <button type="submit">LED Off</button>
          </form>

          <p><a href="/status">View JSON status</a></p>
          <p>Reload this page to refresh the displayed timer state.</p>
        </body>
        </html>
      HTML
    end

    def heartbeat
      return unless @heartbeat_led
      return unless now_ms - @last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS

      @heartbeat_led.write(@heartbeat_led.high? ? 0 : 1)
      @last_heartbeat_ms = now_ms
    end

    def ensure_ap
      return unless now_ms - @last_ap_ensure_ms >= AP_ENSURE_INTERVAL_MS

      unless CYW43.ap_active?
        puts "AP down -> restarting"
        CYW43.enable_ap_mode(AP_SSID, AP_PASSWORD)
      end

      @last_ap_ensure_ms = now_ms
    end

    def now_ms
      Machine.board_millis
    end

    def build_alert_led
      GPIO.new(ALERT_LED_PIN, GPIO::OUT)
    rescue => e
      puts "External LED unavailable on GP#{ALERT_LED_PIN}: #{e.message}"
      nil
    end

    def parse_request_line(request_line)
      parts = request_line.to_s.split(" ")
      [parts[0] || "GET", parts[1] || "/"]
    end

    def parse_request_target(target)
      path, query = target.split("?", 2)
      [path || "/", parse_www_form(query)]
    end

    def read_http_request(client)
      buffer = +""
      header_text = nil
      body = +""
      content_length = 0
      deadline_ms = now_ms + 1500

      while now_ms < deadline_ms
        chunk = client.read_nonblock(512)
        if chunk
          if header_text
            body << chunk
          else
            buffer << chunk
          end
        end

        if !header_text
          split = split_http_message(buffer)
          if split
            header_text, body = split
            content_length = parse_content_length(header_text)
            return parse_header_block(header_text, body) if body.bytesize >= content_length
          end
        elsif body.bytesize >= content_length
          return parse_header_block(header_text, body)
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

    def split_http_message(buffer)
      if (index = buffer.index("\r\n\r\n"))
        header_text = buffer.byteslice(0, index)
        body = buffer.byteslice(index + 4, buffer.bytesize - index - 4) || +""
        [header_text, body]
      elsif (index = buffer.index("\n\n"))
        header_text = buffer.byteslice(0, index)
        body = buffer.byteslice(index + 2, buffer.bytesize - index - 2) || +""
        [header_text, body]
      end
    end

    def parse_header_block(header_text, body)
      lines = header_text.split("\r\n")
      lines = header_text.split("\n") if lines.length == 1

      request_line = lines.shift
      headers = {}
      lines.each do |line|
        key, value = line.split(":", 2)
        next unless key && value

        headers[key.downcase] = value.strip
      end

      [request_line, headers, body]
    end

    def parse_content_length(header_text)
      header_text.each_line do |line|
        key, value = line.split(":", 2)
        next unless key && value
        return value.to_i if key.downcase == "content-length"
      end
      0
    end

    def parse_www_form(data)
      params = {}
      return params unless data
      return params if data.empty?

      data.split("&").each do |pair|
        key, value = pair.split("=", 2)
        next unless key

        params[url_decode(key)] = url_decode(value.to_s)
      end
      params
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

    def write_json(client, payload)
      write_response(client, "200 OK", "application/json", JSON.generate(payload))
    end

    def write_action_response(client, method, message)
      if method == "POST"
        write_json(client, status_payload)
      else
        write_response(client, "200 OK", "text/html; charset=utf-8", action_page(message))
      end
    end

    def action_page(message)
      state = status_payload

      <<~HTML
        <!doctype html>
        <html>
        <head>
          <meta charset="utf-8">
          <meta name="viewport" content="width=device-width,initial-scale=1">
          <title>PICO TIMER</title>
        </head>
        <body>
          <h1>PICO TIMER</h1>
          <p>#{message}</p>
          <p>Running: #{state["running"]}</p>
          <p>Remaining: #{state["remaining"]} sec</p>
          <p>Alert LED: #{state["led"] == 1 ? "on" : "off"}</p>
          <p><a href="/">Back to timer</a></p>
          <p><a href="/status">View JSON status</a></p>
        </body>
        </html>
      HTML
    end

    def safe_error_response(client)
      return unless client

      begin
        write_response(client, "500 Internal Server Error", "text/plain", "Internal Server Error\n")
      rescue => e
        puts "PICO TIMER error response failed: #{e.class}: #{e.message}"
      end
    end

    def write_response(client, status, content_type, body)
      client.write "HTTP/1.1 #{status}\r\n"
      client.write "Content-Type: #{content_type}\r\n"
      client.write "Content-Length: #{body.bytesize}\r\n"
      client.write "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
      client.write "Pragma: no-cache\r\n"
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
  end
end

PicoTimerApp.boot
PicoTimerApp.serve
