#require 'task'

class BLE
  CYW43_WL_GPIO_LED_PIN = 0
  ATT_PROPERTY_READ = 0x02
  GAP_DEVICE_NAME_UUID = 0x2a00
  GATT_PRIMARY_SERVICE_UUID = 0x2800
  GATT_CHARACTERISTIC_UUID = 0x2803
  GAP_SERVICE_UUID = 0x1800

  def self.bd_addr_to_str(addr)
    addr.bytes.map{|b| sprintf("%02X", b)}.join(":")
  end

  class Utils
    def self.uuid(value)
      case value
      when Integer
        str = int16_to_little_endian(value)
      when String
        # Simply trustily assumes "cd833ba3-97c5-4615-a2a0-a6c3e56b24b2" format
        str = "_" * 16
        i = 0
        15.downto(0) do |j|
          i += 1 if value[i] == "-"
          c = value[i, 2].to_s
          if c.length != 2
            raise ArgumentError, "invalid uuid value: `#{value}`"
          end
          if valid_char_for_uuid?(c[0]) && valid_char_for_uuid?(c[1])
            str[j] = c.to_i(16).chr
          else
            str = ""
            break 0
          end
          i += 2
        end
      else
        str = ""
      end
      if str.empty?
        raise ArgumentError, "invalid uuid value: `#{value}`"
      end
      str
    end

    def self.int16_to_little_endian(value)
      if 65536 < value
        raise ArgumentError, "invalid value: `#{value}`"
      end
      (value & 0xff).chr + (value >> 8).chr
    end

    # private

    def self.valid_char_for_uuid?(c)
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

  class GattDatabase
    def initialize
      @data = 0x01.chr
      @handle = 0
    end

    def self.build(&block)
      gatt_db = self.new
      block.call(gatt_db)
      gatt_db.data
    end

    attr_reader :data

    def handle
      @handle += 1
    end

    def next_handle
      @handle + 1
    end

    def add_line(line)
      @data << Utils.int16_to_little_endian(line.length + 2) << line
    end

    def add_service(uuid)
      line = Utils.int16_to_little_endian(BLE::ATT_PROPERTY_READ)
      line << Utils.int16_to_little_endian(handle)
      line << Utils.int16_to_little_endian(BLE::GATT_PRIMARY_SERVICE_UUID)
      line << Utils.int16_to_little_endian(uuid)
      add_line(line)
      yield self if block_given?
    end

    def add_characteristic(uuid, properties, flags, *values)
      # declaration
      line = Utils.int16_to_little_endian(BLE::ATT_PROPERTY_READ)
      line << Utils.int16_to_little_endian(handle)
      line << Utils.int16_to_little_endian(BLE::GATT_CHARACTERISTIC_UUID)
      line << (properties & 0xff).chr
      line << Utils.int16_to_little_endian(next_handle)
      line << Utils.int16_to_little_endian(uuid)
      add_line(line)
      # value
      line = Utils.int16_to_little_endian(flags)
      line << Utils.int16_to_little_endian(handle)
      line << Utils.int16_to_little_endian(uuid)
      values.each do |value|
        case value
        when String
          line << value
        when Integer
          if value < 256
            line << value.chr
          else
            line << Utils.int16_to_little_endian(value)
          end
        else
          raise ArgumentError, "invalid value: `#{value}`"
        end
      end
      add_line(line)
      yield self if block_given?
    end

    def add_descriptor(uuid, properties, flags, value)
      line = Utils.int16_to_little_endian(properties)
      line << Utils.int16_to_little_endian(handle)
      line << Utils.int16_to_little_endian(uuid)
      line << value
      add_line(line)
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

