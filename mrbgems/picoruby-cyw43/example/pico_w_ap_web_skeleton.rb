require "cyw43"
require "json"
require "machine"
require "socket"

module PicoTimerSkeleton
  AP_SSID = "PICO-TIMER"
  AP_PASSWORD = "12345678"
  DEFAULT_SECONDS = 60
  MAX_SECONDS = 60 * 60
  RESPONSE_CHUNK_SIZE = 512

  class << self
    def boot
      CYW43.init("JP")
      CYW43.enable_ap_mode(AP_SSID, AP_PASSWORD)

      puts "AP started"
      puts "AP active?: #{CYW43.ap_active?}"
      puts "AP SSID: #{CYW43.ap_ssid || 'unknown'}"
      puts "AP IP: #{CYW43.ap_ipv4_address || 'unassigned'}"
      puts "AP stations: #{CYW43.ap_station_count}/#{CYW43.ap_max_stations}"

      @timer_running = false
      @timer_duration_ms = DEFAULT_SECONDS * 1000
      @timer_deadline_ms = 0
    end

    def serve
      server = TCPServer.new(nil, 80)
      puts "HTTP server listening on port 80"
      puts "Open: http://#{CYW43.ap_ipv4_address}/"

      loop do
        client = server.accept

        begin
          request_line = client.gets
          drain_headers(client)
          puts "Request: #{request_line.inspect}" if request_line
          path, params = parse_request_target(request_line)
          handle_request(client, path, params)
        rescue => e
          puts "AP web skeleton error: #{e.class}: #{e.message}"
          write_response(client, "500 Internal Server Error", "text/plain", "Internal Server Error\n")
        ensure
          client.close
        end
      end
    end

    def handle_request(client, path, params)
      case path
      when "/"
        write_response(client, "200 OK", "text/html; charset=utf-8", html_page)
      when "/status"
        write_response(client, "200 OK", "application/json", JSON.generate(status_payload))
      when "/start"
        start_timer(params["seconds"])
        write_response(client, "200 OK", "application/json", JSON.generate(status_payload))
      when "/stop"
        stop_timer
        write_response(client, "200 OK", "application/json", JSON.generate(status_payload))
      else
        write_response(client, "404 Not Found", "text/plain", "Not Found\n")
      end
    end

    def start_timer(seconds_value)
      seconds = seconds_value ? seconds_value.to_i : DEFAULT_SECONDS
      seconds = DEFAULT_SECONDS if seconds <= 0
      seconds = MAX_SECONDS if seconds > MAX_SECONDS

      @timer_duration_ms = seconds * 1000
      @timer_deadline_ms = now_ms + @timer_duration_ms
      @timer_running = true
    end

    def stop_timer
      @timer_running = false
      @timer_deadline_ms = 0
    end

    def timer_running?
      if @timer_running && @timer_deadline_ms <= now_ms
        stop_timer
      end
      @timer_running
    end

    def remaining_ms
      return 0 unless timer_running?

      remaining = @timer_deadline_ms - now_ms
      remaining > 0 ? remaining : 0
    end

    def status_payload
      {
        "ap_active" => CYW43.ap_active?,
        "ap_ssid" => CYW43.ap_ssid,
        "ap_ip" => CYW43.ap_ipv4_address,
        "stations" => CYW43.ap_station_count,
        "stations_max" => CYW43.ap_max_stations,
        "running" => timer_running?,
        "remaining_ms" => remaining_ms,
        "remaining_seconds" => (remaining_ms / 1000.0).ceil,
        "default_seconds" => DEFAULT_SECONDS,
        "max_seconds" => MAX_SECONDS
      }
    end

    def html_page
      current = status_payload

      <<~HTML
        <!DOCTYPE html>
        <html>
        <head>
          <meta charset="utf-8">
          <meta name="viewport" content="width=device-width, initial-scale=1">
          <title>PicoRuby Timer Skeleton</title>
        </head>
        <body>
          <h1>PicoRuby Timer Skeleton</h1>
          <p>SSID: #{current["ap_ssid"]}</p>
          <p>AP IP: #{current["ap_ip"]}</p>
          <p>Stations: #{current["stations"]}/#{current["stations_max"]}</p>
          <p>Running: #{current["running"]}</p>
          <p>Remaining: #{current["remaining_seconds"]} sec</p>

          <form action="/start" method="get">
            <label>
              Seconds:
              <input name="seconds" type="number" min="1" max="#{MAX_SECONDS}" value="#{DEFAULT_SECONDS}">
            </label>
            <button type="submit">Start</button>
          </form>

          <form action="/stop" method="get">
            <button type="submit">Stop</button>
          </form>

          <p><a href="/status">View JSON status</a></p>
          <p>Reload this page to refresh the displayed timer state.</p>
        </body>
        </html>
      HTML
    end

    def now_ms
      Machine.board_millis
    end

    def parse_request_target(request_line)
      return ["/", {}] if request_line.nil?

      parts = request_line.split(" ")
      target = parts[1] || "/"
      path, query = target.split("?", 2)
      [path, parse_query(query)]
    end

    def parse_query(query)
      params = {}
      return params unless query

      query.split("&").each do |pair|
        key, value = pair.split("=", 2)
        next unless key

        params[key] = (value || "").tr("+", " ")
      end
      params
    end

    def drain_headers(client)
      while (line = client.gets)
        break if line == "\r\n" || line == "\n"
      end
    end

    def write_response(client, status, content_type, body)
      client.write "HTTP/1.1 #{status}\r\n"
      client.write "Content-Type: #{content_type}\r\n"
      client.write "Content-Length: #{body.bytesize}\r\n"
      client.write "Connection: close\r\n"
      client.write "\r\n"
      offset = 0
      body_size = body.bytesize
      while offset < body_size
        chunk = body.byteslice(offset, RESPONSE_CHUNK_SIZE)
        break unless chunk

        client.write chunk
        offset += chunk.bytesize
      end
    end
  end
end

PicoTimerSkeleton.boot
PicoTimerSkeleton.serve
