#!/usr/bin/env ruby

# CRuby WebSocket server for PicoRuby interop testing
#
# Usage:
#   Terminal 1: ruby cruby_server.rb
#   Terminal 2: picoruby picoruby_client.rb
#
# Required gem:
#   gem install em-websocket

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'em-websocket'
  gem 'base64'
end

PORT = 8080
HOST = '0.0.0.0'

puts "Starting CRuby WebSocket server on ws://#{HOST}:#{PORT}"
puts "Press Ctrl+C to stop"

EM.run {
  EM::WebSocket.run(host: HOST, port: PORT) do |ws|
    ws.onopen do |handshake|
      puts "[Server] Client connected from #{handshake.origin}"
    end

    ws.onmessage do |message|
      puts "[Server] Received text: #{message.inspect}"

      # Echo server - send back what we received
      ws.send(message)
      puts "[Server] Echoed text: #{message.inspect}"

      # Special commands for testing
      case message
      when "PING_TEST"
        ws.ping("ping_payload")
        puts "[Server] Sent ping with payload"
      when "CLOSE_TEST"
        ws.close(1000, "Normal closure")
        puts "[Server] Initiated close"
      when "BINARY_TEST"
        binary_data = "\x00\x01\x02\x03\x04\x05"
        ws.send(binary_data)
        puts "[Server] Sent binary test data"
      end
    end

    ws.onbinary do |data|
      puts "[Server] Received binary: #{data.bytes.inspect} (#{data.length} bytes)"

      # Echo back binary data
      ws.send_binary(data)
      puts "[Server] Echoed binary: #{data.length} bytes"
    end

    ws.onping do |message|
      puts "[Server] Received ping: #{message.inspect}"
    end

    ws.onpong do |message|
      puts "[Server] Received pong: #{message.inspect}"
    end

    ws.onclose do
      puts "[Server] Connection closed"
    end

    ws.onerror do |error|
      puts "[Server] Error: #{error.message}"
    end
  end

  puts "Server ready!"
}
