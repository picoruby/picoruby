#!/usr/bin/env ruby
# Minimal MQTT 3.1.1 broker for testing PicoRuby client
#
# Usage:
#   Terminal 1: ruby cruby_server.rb
#   Terminal 2: picoruby picoruby_client.rb

require 'socket'
require 'thread'

PORT = 1883

def encode_length(length)
  result = ""
  loop do
    byte = length % 128
    length /= 128
    byte |= 0x80 if length > 0
    result << byte.chr
    break if length == 0
  end
  result
end

def read_remaining_length(socket)
  multiplier = 1
  value = 0
  loop do
    byte = socket.read(1)&.ord
    raise EOFError if byte.nil?
    value += (byte & 0x7F) * multiplier
    break if (byte & 0x80) == 0
    multiplier *= 128
  end
  value
end

def encode_string(str)
  [str.bytesize].pack("n") + str
end

def topic_matches?(filter, topic)
  filter_parts = filter.split("/")
  topic_parts  = topic.split("/")
  fi = 0
  ti = 0
  while fi < filter_parts.size
    fp = filter_parts[fi]
    if fp == "#"
      return true
    elsif fp == "+"
      return false if ti >= topic_parts.size
    elsif fp != topic_parts[ti]
      return false
    end
    fi += 1
    ti += 1
  end
  ti == topic_parts.size
end

@mutex = Mutex.new
@subscriptions = {}

def route_publish(topic, payload)
  pub_payload = encode_string(topic) + payload
  frame = "\x30" + encode_length(pub_payload.bytesize) + pub_payload
  @mutex.synchronize do
    @subscriptions.each do |sock, filters|
      filters.each do |filter|
        if topic_matches?(filter, topic)
          sock.write(frame) rescue nil
          break
        end
      end
    end
  end
end

def handle_client(socket)
  puts "[Broker] Client connected from #{socket.remote_address.ip_address}"
  @mutex.synchronize { @subscriptions[socket] = [] }

  loop do
    byte1 = socket.read(1)
    break if byte1.nil?

    packet_type = (byte1.ord >> 4) & 0x0F
    remaining   = read_remaining_length(socket)
    data        = remaining > 0 ? socket.read(remaining) : ""
    break if data.nil?

    case packet_type
    when 1  # CONNECT
      socket.write("\x20\x02\x00\x00")
      puts "[Broker] CONNACK sent"

    when 3  # PUBLISH
      topic_len = data[0, 2].unpack1("n")
      topic     = data[2, topic_len]
      payload   = data[2 + topic_len..]
      puts "[Broker] PUBLISH #{topic}: #{payload.inspect}"
      route_publish(topic, payload)

    when 8  # SUBSCRIBE
      packet_id = data[0, 2].unpack1("n")
      offset    = 2
      new_filters = []
      while offset < data.bytesize
        flen   = data[offset, 2].unpack1("n")
        offset += 2
        filter = data[offset, flen]
        offset += flen + 1  # skip QoS byte
        new_filters << filter
        puts "[Broker] SUBSCRIBE #{filter}"
      end
      @mutex.synchronize { @subscriptions[socket].concat(new_filters) }
      suback = [packet_id].pack("n") + "\x00" * new_filters.size
      socket.write("\x90" + encode_length(suback.bytesize) + suback)

    when 12  # PINGREQ
      socket.write("\xD0\x00")
      puts "[Broker] PINGRESP sent"

    when 14  # DISCONNECT
      puts "[Broker] Client disconnected gracefully"
      break
    end
  end
rescue => e
  puts "[Broker] Error: #{e.class}: #{e.message}"
ensure
  @mutex.synchronize { @subscriptions.delete(socket) }
  socket.close rescue nil
  puts "[Broker] Connection closed"
end

server = TCPServer.new("0.0.0.0", PORT)
puts "MQTT broker listening on port #{PORT}"
puts "Press Ctrl+C to stop"

Signal.trap("INT") { puts "\nShutting down"; exit }

loop do
  client = server.accept
  Thread.new(client) { |s| handle_client(s) }
end
