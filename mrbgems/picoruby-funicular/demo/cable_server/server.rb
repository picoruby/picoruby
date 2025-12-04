#!/usr/bin/env ruby

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'eventmachine'
  gem 'em-websocket'
  gem 'json'
  gem 'base64'
end

# ActionCable-compatible WebSocket server for testing
class CableServer
  def initialize
    @clients = []
    @ping_timer = nil
  end

  def start(host = '0.0.0.0', port = 9292)
    puts "Starting ActionCable-compatible test server on ws://#{host}:#{port}/cable"
    puts "Press Ctrl+C to stop"

    EM.run do
      EM::WebSocket.run(host: host, port: port) do |ws|
        ws.onopen do |handshake|
          puts "[Server] Client connected from #{handshake.origin}"
          @clients << ws

          # Send welcome message
          welcome = { type: "welcome" }
          json = JSON.generate(welcome)
          puts "[Server] Sending welcome: #{json}"
          ws.send(json)
        end

        ws.onmessage do |data|
          puts "[Server] Raw data received (class: #{data.class}): #{data.inspect}"
          puts "[Server] Data length: #{data.length}"
          puts "[Server] Data bytes: #{data.bytes.inspect}" if data.respond_to?(:bytes)
          handle_message(ws, data)
        end

        ws.onclose do
          puts "[Server] Client disconnected"
          @clients.delete(ws)
        end

        ws.onerror do |error|
          puts "[Server] Error: #{error.message}"
        end
      end

      # Setup ping timer
      @ping_timer = EM.add_periodic_timer(3) do
        ping = { type: "ping", message: Time.now.to_i }
        json = JSON.generate(ping)
        @clients.each do |client|
          client.send(json) rescue nil
        end
      end

      # Graceful shutdown
      Signal.trap("INT") do
        puts "\n[Server] Shutting down..."
        EM.stop
      end

      Signal.trap("TERM") do
        puts "\n[Server] Shutting down..."
        EM.stop
      end
    end
  end

  private

  def handle_message(ws, data)
    message = JSON.parse(data)
    command = message["command"]
    identifier = message["identifier"]

    puts "[Server] Received: #{command} - #{identifier}"

    case command
    when "subscribe"
      # Confirm subscription
      response = {
        type: "confirm_subscription",
        identifier: identifier
      }
      ws.send(JSON.generate(response))
      puts "[Server] Subscription confirmed: #{identifier}"

    when "unsubscribe"
      puts "[Server] Unsubscribed: #{identifier}"

    when "message"
      # Handle message/perform action
      data_payload = JSON.parse(message["data"])
      action = data_payload["action"]

      puts "[Server] Action: #{action}, Data: #{data_payload.inspect}"

      # Echo back the message to all clients subscribed to this channel
      response = {
        identifier: identifier,
        message: {
          action: action,
          data: data_payload,
          timestamp: Time.now.to_i
        }
      }

      broadcast(identifier, response)
    end
  rescue JSON::ParserError => e
    puts "[Server] JSON parse error: #{e.message}"
  rescue => e
    puts "[Server] Error in handle_message: #{e.class}: #{e.message}"
  end

  def broadcast(identifier, message)
    json = JSON.generate(message)
    @clients.each do |client|
      client.send(json) rescue nil
    end
    puts "[Server] Broadcasted to #{@clients.size} clients"
  end
end

# Start server if run directly
if __FILE__ == $0
  server = CableServer.new
  server.start
end
