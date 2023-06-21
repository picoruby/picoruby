require 'task'

class BLE
  CYW43_WL_GPIO_LED_PIN = 0

  def self.bd_addr_to_str(addr)
    addr.bytes.map{|b| sprintf("%02X", b)}.join(":")
  end

  class UUID
    def initialize(value)
      case value
      when Integer
        @type = :uuid16
        @str = ""
        if value < 65536
          @str << (value & 0xff).chr
          @str << ((value >> 8) & 0xff).chr
        end
      when String
        # Simply trustily assumes "cd833ba3-97c5-4615-a2a0-a6c3e56b24b2" format
        @type = :uuid128
        @str = "_" * 16
        i = 0
        15.downto(0) do |j|
          i += 1 if value[i] == "-"
          c = value[i, 2].to_s
          if c.length != 2
            raise ArgumentError, "invalid uuid value: `#{value}`"
          end
          if valid_char?(c[0]) && valid_char?(c[1])
            @str[j] = c.to_i(16).chr
          else
            @str = ""
            break 0
          end
          i += 2
        end
      end
      if @str.empty?
        raise ArgumentError, "invalid uuid value: `#{value}`"
      end
    end

    attr_reader :type

    def to_s
      @str
    end

    # private

    def valid_char?(c)
      return false unless c
      o = c.ord
      ('0'.ord..'9'.ord) === o || ('a'.ord..'z'.ord) === o || ('A'.ord..'Z'.ord) === o
    end
  end

  class AttServer
    def initialize
      @connections = []
      init
    end

    def start
      hci_power_on
      @task = Task.new(self) do |server|
        while true
          if server.heartbeat_on?
            server.heartbeat_callback
            server.heartbeat_off
          end
          if event = server.packet_event
            server.packet_callback(event)
            server.down_packet_flag
          end
          #if (conn_handle, att_handle, offset, buffer = server._read_event)
          #  server.read_callback(conn_handle, att_handle, offset, buffer)
          #end
          #if (conn_handle, att_handle, offset, buffer = server._write_event)
          #  server.write_callback(conn_handle, att_handle, offset, buffer)
          #end
          sleep_ms 50
        end
      end
      return 0
    end

    def stop
      @task.suspend
      return 0
    end
  end

  class AdvertisingData
    def initialize
      @data = ""
    end

    attr_reader :data

    def add(type, *data)
      length_pos = @data.length
      @data << 0.chr # dummy length
      @data << type.chr
      length = 1
      data.each do |d|
        case d
        when String
          @data << d
          length += d.length
        when Integer
          while 0 < d
            @data << (d & 0xff).chr
            d >>= 8
            length += 1
          end
        else
          raise ArgumentError, "invalid data type: `#{d}`"
        end
      end
      @data[length_pos] = length.chr
    end

    # private_constant :Body
    def self.build(&block)
      b = self.new
      block.call(b)
      adv_data = b.data
      if 31 < adv_data.length
        raise ArgumentError, "too long AdvData. It must be less than 32 bytes."
      end
      adv_data
    end
  end
end

