#
# mDNS library for PicoRuby
# This is a pure Ruby implementation of mDNS (Multicast DNS).
# It provides both client and responder functionality.
#
# Author: Hitoshi HASUMI
# License: MIT
#

require 'socket'

module MDNS
  class MDNSError < StandardError; end
  class ParseError < MDNSError; end
  class EncodeError < MDNSError; end

  MDNS_PORT = 5353
  MDNS_ADDR = "224.0.0.251"

  # DNS Resource Record Types
  TYPE_A = 1
  TYPE_PTR = 12
  TYPE_TXT = 16
  TYPE_SRV = 33
  TYPE_ANY = 255

  # DNS Class
  CLASS_IN = 1
  CLASS_ANY = 255

  # mDNS specific: Cache flush bit
  CACHE_FLUSH = 0x8000

  # DNS Message class for encoding/decoding DNS packets
  class Message
    attr_accessor :id, :qr, :opcode, :aa, :tc, :rd, :ra, :rcode
    attr_accessor :questions, :answers, :authorities, :additionals

    def initialize
      @id = 0
      @qr = 0        # 0 = query, 1 = response
      @opcode = 0    # 0 = standard query
      @aa = 0        # Authoritative Answer
      @tc = 0        # Truncation
      @rd = 0        # Recursion Desired (should be 0 for mDNS)
      @ra = 0        # Recursion Available
      @rcode = 0     # Response code
      @questions = []
      @answers = []
      @authorities = []
      @additionals = []
    end

    def encode
      # Encode header
      flags = (@qr << 15) | (@opcode << 11) | (@aa << 10) |
              (@tc << 9) | (@rd << 8) | (@ra << 7) | @rcode

      header = [@id, flags, @questions.length, @answers.length,
                @authorities.length, @additionals.length].pack("n6")

      result = header

      # Encode questions
      @questions.each do |q|
        result += q.encode
      end

      # Encode answers
      @answers.each do |rr|
        result += rr.encode
      end

      # Encode authorities
      @authorities.each do |rr|
        result += rr.encode
      end

      # Encode additionals
      @additionals.each do |rr|
        result += rr.encode
      end

      result
    end

    def self.decode(data)
      msg = Message.new
      offset = 0

      # Decode header (12 bytes)
      if data.length < 12
        raise ParseError.new("Message too short")
      end

      header = data[0, 12].unpack("n6")
      msg.id = header[0]
      flags = header[1]
      msg.qr = (flags >> 15) & 1
      msg.opcode = (flags >> 11) & 0xF
      msg.aa = (flags >> 10) & 1
      msg.tc = (flags >> 9) & 1
      msg.rd = (flags >> 8) & 1
      msg.ra = (flags >> 7) & 1
      msg.rcode = flags & 0xF

      qd_count = header[2]
      an_count = header[3]
      ns_count = header[4]
      ar_count = header[5]

      offset = 12

      # Decode questions
      qd_count.times do
        q, offset = Question.decode(data, offset)
        msg.questions << q
      end

      # Decode answers
      an_count.times do
        rr, offset = ResourceRecord.decode(data, offset)
        msg.answers << rr
      end

      # Decode authorities
      ns_count.times do
        rr, offset = ResourceRecord.decode(data, offset)
        msg.authorities << rr
      end

      # Decode additionals
      ar_count.times do
        rr, offset = ResourceRecord.decode(data, offset)
        msg.additionals << rr
      end

      msg
    end
  end

  # DNS Question class
  class Question
    attr_accessor :qname, :qtype, :qclass

    def initialize(qname, qtype, qclass = CLASS_IN)
      @qname = qname
      @qtype = qtype
      @qclass = qclass
    end

    def encode
      name_encoded = encode_name(@qname)
      name_encoded + [@qtype, @qclass].pack("n2")
    end

    def self.decode(data, offset)
      qname, offset = decode_name(data, offset)
      qtype, qclass = data[offset, 4].unpack("n2")
      [Question.new(qname, qtype, qclass), offset + 4]
    end

    private

    def encode_name(name)
      result = ""
      name.split('.').each do |label|
        result += [label.length].pack("C") + label
      end
      result += [0].pack("C")  # End of name
      result
    end

    def self.decode_name(data, offset)
      labels = []
      original_offset = offset
      jumped = false
      max_jumps = 20
      jumps = 0

      loop do
        if offset >= data.length
          raise ParseError.new("Unexpected end of data while decoding name")
        end

        len = data[offset].ord

        if len == 0
          offset += 1
          break
        elsif (len & 0xC0) == 0xC0
          # Pointer
          if offset + 1 >= data.length
            raise ParseError.new("Invalid pointer in name")
          end
          pointer = ((len & 0x3F) << 8) | data[offset + 1].ord
          offset += 2 unless jumped
          jumped = true
          offset = pointer
          jumps += 1
          if jumps > max_jumps
            raise ParseError.new("Too many jumps in name")
          end
        else
          # Label
          if offset + len >= data.length
            raise ParseError.new("Label extends beyond data")
          end
          label = data[offset + 1, len]
          labels << label
          offset += len + 1
        end
      end

      offset = original_offset + 2 if jumped
      [labels.join('.'), offset]
    end
  end

  # DNS Resource Record class
  class ResourceRecord
    attr_accessor :name, :rtype, :rclass, :ttl, :rdata

    def initialize(name, rtype, rclass, ttl, rdata)
      @name = name
      @rtype = rtype
      @rclass = rclass
      @ttl = ttl
      @rdata = rdata
    end

    def encode
      name_encoded = encode_name(@name)
      rdata_encoded = encode_rdata
      rdlength = rdata_encoded.length

      name_encoded +
        [@rtype, @rclass].pack("n2") +
        [@ttl].pack("N") +
        [rdlength].pack("n") +
        rdata_encoded
    end

    def self.decode(data, offset)
      name, offset = Question.decode_name(data, offset)

      if offset + 10 > data.length
        raise ParseError.new("Not enough data for resource record")
      end

      rtype, rclass, ttl, rdlength = data[offset, 10].unpack("n2Nn")
      offset += 10

      if offset + rdlength > data.length
        raise ParseError.new("RDATA extends beyond message")
      end

      rdata_raw = data[offset, rdlength]
      rdata = decode_rdata(rtype, rdata_raw, data, offset)
      offset += rdlength

      [ResourceRecord.new(name, rtype, rclass, ttl, rdata), offset]
    end

    private

    def encode_name(name)
      result = ""
      name.split('.').each do |label|
        result += [label.length].pack("C") + label
      end
      result += [0].pack("C")
      result
    end

    def encode_rdata
      case @rtype
      when TYPE_A
        # IPv4 address: a.b.c.d
        parts = @rdata.split('.')
        parts.map(&:to_i).pack("C4")
      when TYPE_PTR
        # Domain name
        encode_name(@rdata)
      when TYPE_TXT
        # Text records as array of strings
        result = ""
        if @rdata.is_a?(Array)
          @rdata.each do |txt|
            result += [txt.length].pack("C") + txt
          end
        elsif @rdata.is_a?(Hash)
          @rdata.each do |key, value|
            txt = "#{key}=#{value}"
            result += [txt.length].pack("C") + txt
          end
        else
          txt = @rdata.to_s
          result += [txt.length].pack("C") + txt
        end
        result
      when TYPE_SRV
        # SRV: priority, weight, port, target
        priority = @rdata[:priority] || 0
        weight = @rdata[:weight] || 0
        port = @rdata[:port]
        target = @rdata[:target]
        [priority, weight, port].pack("n3") + encode_name(target)
      else
        @rdata
      end
    end

    def self.decode_rdata(rtype, rdata_raw, full_data, rdata_offset)
      case rtype
      when TYPE_A
        # IPv4 address
        parts = rdata_raw.unpack("C4")
        parts.join('.')
      when TYPE_PTR
        # Domain name
        name, _ = Question.decode_name(full_data, rdata_offset)
        name
      when TYPE_TXT
        # Text records
        result = []
        offset = 0
        while offset < rdata_raw.length
          len = rdata_raw[offset].ord
          offset += 1
          if len > 0 && offset + len <= rdata_raw.length
            result << rdata_raw[offset, len]
            offset += len
          end
        end
        result
      when TYPE_SRV
        # SRV record
        priority, weight, port = rdata_raw[0, 6].unpack("n3")
        target, _ = Question.decode_name(full_data, rdata_offset + 6)
        {
          priority: priority,
          weight: weight,
          port: port,
          target: target
        }
      else
        rdata_raw
      end
    end
  end

  # mDNS Client for querying services
  class Client
    def initialize
      @socket = UDPSocket.new
      @socket.setsockopt(Socket::SOL_SOCKET, Socket::SO_REUSEADDR, 1)
    end

    def query(service_name, timeout: 2)
      msg = Message.new
      msg.id = 0  # mDNS queries typically use ID 0
      msg.qr = 0  # Query
      msg.questions << Question.new(service_name, TYPE_PTR, CLASS_IN)

      packet = msg.encode
      @socket.send(packet, 0, MDNS_ADDR, MDNS_PORT)

      # Collect responses
      results = []
      deadline = Time.now.to_f + timeout

      begin
        while Time.now.to_f < deadline
          remaining = deadline - Time.now.to_f
          break if remaining <= 0

          ready = IO.select([@socket], nil, nil, remaining)
          if ready && ready[0]
            data, addr = @socket.recvfrom(4096)
            response = Message.decode(data)

            if response.qr == 1  # Response
              response.answers.each do |rr|
                results << {
                  name: rr.name,
                  type: rr.rtype,
                  data: rr.rdata,
                  ttl: rr.ttl
                }
              end
            end
          end
        end
      rescue => e
        # Timeout or error
      end

      results
    end

    def resolve(hostname, timeout: 2)
      msg = Message.new
      msg.id = 0
      msg.qr = 0
      msg.questions << Question.new(hostname, TYPE_A, CLASS_IN)

      packet = msg.encode
      @socket.send(packet, 0, MDNS_ADDR, MDNS_PORT)

      deadline = Time.now.to_f + timeout

      begin
        while Time.now.to_f < deadline
          remaining = deadline - Time.now.to_f
          break if remaining <= 0

          ready = IO.select([@socket], nil, nil, remaining)
          if ready && ready[0]
            data, addr = @socket.recvfrom(4096)
            response = Message.decode(data)

            if response.qr == 1
              response.answers.each do |rr|
                if rr.rtype == TYPE_A && rr.name == hostname
                  return rr.rdata
                end
              end
            end
          end
        end
      rescue => e
        # Timeout or error
      end

      nil
    end

    def close
      @socket.close if @socket
    end
  end

  # mDNS Responder for advertising services
  class Responder
    def initialize(hostname, ip_address)
      @hostname = hostname
      @ip_address = ip_address
      @services = []
      @socket = UDPSocket.new
      @socket.setsockopt(Socket::SOL_SOCKET, Socket::SO_REUSEADDR, 1)
      @socket.bind("0.0.0.0", MDNS_PORT)

      # Join multicast group
      mreq = [@ip_address, MDNS_ADDR].map { |ip|
        ip.split('.').map(&:to_i).pack("C4")
      }.join
      @socket.setsockopt(Socket::IPPROTO_IP, Socket::IP_ADD_MEMBERSHIP, mreq)
    end

    def advertise(service_name:, service_type:, port:, txt_records: {})
      @services << {
        name: service_name,
        type: service_type,
        port: port,
        txt: txt_records
      }
    end

    def run
      loop do
        begin
          data, addr = @socket.recvfrom(4096)
          handle_query(data, addr)
        rescue => e
          # Handle error
        end
      end
    end

    def close
      @socket.close if @socket
    end

    private

    def handle_query(data, addr)
      begin
        msg = Message.decode(data)

        # Only handle queries
        return unless msg.qr == 0

        response = Message.new
        response.id = msg.id
        response.qr = 1  # Response
        response.aa = 1  # Authoritative

        msg.questions.each do |q|
          # Check if we can answer this question
          @services.each do |service|
            service_instance = "#{service[:name]}.#{service[:type]}"

            if q.qname == service[:type] && q.qtype == TYPE_PTR
              # Answer with PTR record
              response.answers << ResourceRecord.new(
                service[:type],
                TYPE_PTR,
                CLASS_IN,
                120,
                service_instance
              )

              # Add SRV record in additional section
              response.additionals << ResourceRecord.new(
                service_instance,
                TYPE_SRV,
                CLASS_IN,
                120,
                {
                  priority: 0,
                  weight: 0,
                  port: service[:port],
                  target: @hostname
                }
              )

              # Add A record in additional section
              response.additionals << ResourceRecord.new(
                @hostname,
                TYPE_A,
                CLASS_IN,
                120,
                @ip_address
              )

              # Add TXT record if present
              unless service[:txt].empty?
                response.additionals << ResourceRecord.new(
                  service_instance,
                  TYPE_TXT,
                  CLASS_IN,
                  120,
                  service[:txt]
                )
              end
            elsif q.qname == @hostname && q.qtype == TYPE_A
              # Answer with A record
              response.answers << ResourceRecord.new(
                @hostname,
                TYPE_A,
                CLASS_IN,
                120,
                @ip_address
              )
            end
          end
        end

        # Send response if we have answers
        unless response.answers.empty?
          packet = response.encode
          @socket.send(packet, 0, addr[3], addr[1])
        end
      rescue => e
        # Ignore malformed packets
      end
    end
  end
end
