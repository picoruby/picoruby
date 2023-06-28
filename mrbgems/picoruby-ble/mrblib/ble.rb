require 'task'
require 'mbedtls'

class BLE
  # GATT Characteristic Properties
  BROADCAST =                   0x01
  READ =                        0x02
  WRITE_WITHOUT_RESPONSE =      0x04
  WRITE =                       0x08
  NOTIFY =                      0x10
  INDICATE =                    0x20
  AUTHENTICATED_SIGNED_WRITE =  0x40
  EXTENDED_PROPERTIES =         0x80
  # custom BTstack extension
  DYNAMIC =                     0x100
  LONG_UUID =                   0x200

  # read permissions
  READ_PERMISSION_BIT_0 =       0x400
  READ_PERMISSION_BIT_1 =       0x800

  ENCRYPTION_KEY_SIZE_7 =       0x6000
  ENCRYPTION_KEY_SIZE_8 =       0x7000
  ENCRYPTION_KEY_SIZE_9 =       0x8000
  ENCRYPTION_KEY_SIZE_10 =      0x9000
  ENCRYPTION_KEY_SIZE_11 =      0xa000
  ENCRYPTION_KEY_SIZE_12 =      0xb000
  ENCRYPTION_KEY_SIZE_13 =      0xc000
  ENCRYPTION_KEY_SIZE_14 =      0xd000
  ENCRYPTION_KEY_SIZE_15 =      0xe000
  ENCRYPTION_KEY_SIZE_16 =      0xf000
  ENCRYPTION_KEY_SIZE_MASK =    0xf000

  # only used by gatt compiler >= 0xffff
  # Extended Properties
  RELIABLE_WRITE =              0x00010000
  AUTHENTICATION_REQUIRED =     0x00020000
  AUTHORIZATION_REQUIRED =      0x00040000
  READ_ANYBODY =                0x00080000
  READ_ENCRYPTED =              0x00100000
  READ_AUTHENTICATED =          0x00200000
  READ_AUTHENTICATED_SC =       0x00400000
  READ_AUTHORIZED =             0x00800000
  WRITE_ANYBODY =               0x01000000
  WRITE_ENCRYPTED =             0x02000000
  WRITE_AUTHENTICATED =         0x04000000
  WRITE_AUTHENTICATED_SC =      0x08000000
  WRITE_AUTHORIZED =            0x10000000

  # Broadcast, Notify, Indicate, Extended Properties are only used to describe a GATT Characteristic, but are free to use with att_db
  # - write permissions
  WRITE_PERMISSION_BIT_0 =      0x01
  WRITE_PERMISSION_BIT_1 =      0x10
  # - SC required
  READ_PERMISSION_SC =          0x20
  WRITE_PERMISSION_SC =         0x80

  CYW43_WL_GPIO_LED_PIN = 0
  GAP_DEVICE_NAME_UUID = 0x2a00
  GATT_PRIMARY_SERVICE_UUID = 0x2800
  GATT_SECONDARY_SERVICE_UUID = 0x2801
  GATT_CHARACTERISTIC_UUID = 0x2803
  GAP_SERVICE_UUID = 0x1800
  GATT_SERVICE_UUID = 0x1801
  CLIENT_CHARACTERISTIC_CONFIGURATION = 0x2902
  CHARACTERISTIC_DATABASE_HASH = 0x2b2a

  class Utils
    def self.bd_addr_to_str(addr)
      addr.bytes.map{|b| sprintf("%02X", b)}.join(":")
    end

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
      #if 65536 < value
      #  raise ArgumentError, "invalid value: `#{value}`"
      #end
      (value & 0xff).chr + (value >> 8 & 0xff).chr
    end

    # private

    def self.valid_char_for_uuid?(c)
      o = c&.downcase
      ('0'..'9') === o || ('a'..'f') === o
    end

  end

  class AttServer
    def initialize(profile)
      @connections = []
      init(profile)
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
    ATT_DB_VERSION = 0x01
    def initialize(&block)
      @profile_data = ATT_DB_VERSION.chr
      @handle = 0
      @hash_src = ""
      @hash_pos = nil
      @database_hash_key = "\x00" * 16
      block.call self
      insert_database_hash
      @profile_data << "\x00\x00"
    end

    attr_reader :profile_data
    attr_reader :hash_src
    attr_accessor :database_hash_key

    def insert_database_hash
      return unless @hash_pos
      # @type ivar @hash_pos: Integer
      cmac = MbedTLS::CMAC.new(@database_hash_key, 'AES')
      cmac.update(@hash_src)
      digest = cmac.digest
      0.upto(15) do |i|
        (c = digest[15 - i]) or raise "digest[#{15 - i}] is nil"
        @profile_data[@hash_pos + i] = c
      end
    end

    def push_handle
      @handle += 1
    end

    def add_line(line)
      @profile_data << Utils.int16_to_little_endian(line.length + 2) << line
    end

    def add_service(service, uuid)
      line = Utils.int16_to_little_endian(READ)
      [
        Utils.int16_to_little_endian(push_handle),
        Utils.int16_to_little_endian(service),
        uuid2str(uuid)
      ].each do |element|
        line << element
        @hash_src << element
      end
      add_line(line)
      yield self if block_given?
    end

    def add_characteristic(uuid, properties, *values)
      # declaration
      line = Utils.int16_to_little_endian(READ)
      [
        Utils.int16_to_little_endian(push_handle),
        Utils.int16_to_little_endian(GATT_CHARACTERISTIC_UUID),
        (properties & 0xff).chr,
        Utils.int16_to_little_endian(@handle + 1),
        uuid2str(uuid)
      ].each do |element|
        line << element
        @hash_src << element
      end
      add_line(line)
      # value
      value_flags = att_flags(properties)
      if uuid.is_a?(String) && uuid.length == 16
        value_flags = value_flags | LONG_UUID
      end
      line = Utils.int16_to_little_endian(value_flags)
      line << Utils.int16_to_little_endian(push_handle)
      line << uuid2str(uuid)
      if uuid == CHARACTERISTIC_DATABASE_HASH
        values = Array.new(16, 0)
        @hash_pos = @profile_data.length + line.length + 2
      end
      values.each do |value|
        line << case value
        when String
          value
        when Integer
          value < 256 ? value.chr : Utils.int16_to_little_endian(value)
        end
      end
      add_line(line)
      if properties & (NOTIFY | INDICATE) != 0
        # characteristic configuration
        flags = write_permissions_and_key_size_flags_from_properties(properties)
        flags |= READ
        flags |= WRITE
        flags |= WRITE_WITHOUT_RESPONSE
        flags |= DYNAMIC
        line = Utils.int16_to_little_endian(flags)
        [
          Utils.int16_to_little_endian(push_handle),
          Utils.int16_to_little_endian(CLIENT_CHARACTERISTIC_CONFIGURATION)
        ].each do |element|
          line << element
          @hash_src << element
        end
        line << 0.chr * 2
        add_line(line)
      end
      yield self if block_given?
    end

    def add_descriptor(uuid, properties, *values)
      line = Utils.int16_to_little_endian(properties)
      line << Utils.int16_to_little_endian(push_handle)
      line << uuid2str(uuid)
      values.each do |value|
        line << case value
        when String
          value
        when Integer
          value < 256 ? value.chr : Utils.int16_to_little_endian(value)
        end
      end
      add_line(line)
    end

    # private

    def att_flags(properties)
      # drop Broadcast (0x01), Notify (0x10), Indicate (0x20), Extended Properties (0x80) - not used for flags
      #properties &= 0xffffff4e
      properties &= 0xfffff4e
      # 0x1ff80000 =  READ_AUTHORIZED |
      #               READ_AUTHENTICATED_SC |
      #               READ_AUTHENTICATED |
      #               READ_ENCRYPTED |
      #               READ_ANYBODY |
      #               WRITE_AUTHORIZED |
      #               WRITE_AUTHENTICATED |
      #               WRITE_AUTHENTICATED_SC |
      #               WRITE_ENCRYPTED |
      #               WRITE_ANYBODY
      distinct_permissions_used = (properties & 0x1ff80000 != 0)
      encryption_key_size_specified = (properties & ENCRYPTION_KEY_SIZE_MASK) != 0

      # if distinct permissions not used and encyrption key size specified -> set READ/WRITE Encrypted
      if encryption_key_size_specified && !distinct_permissions_used
        properties |= READ_ENCRYPTED | WRITE_ENCRYPTED
      end
      # if distinct permissions not used and authentication is requires -> set READ/WRITE Authenticated
      if 0 < properties & AUTHENTICATION_REQUIRED && !distinct_permissions_used
        properties |= READ_AUTHENTICATED | WRITE_AUTHENTICATED
      end
      # if distinct permissions not used and authorized is requires -> set READ/WRITE Authorized
      if 0 < properties & AUTHORIZATION_REQUIRED && !distinct_permissions_used
        properties |= READ_AUTHORIZED | WRITE_AUTHORIZED
      end

      # determine read/write security requirements
      read_requires_sc, write_requires_sc = false, false
      read_security_level = if 0 < properties & READ_AUTHORIZED
        3
      elsif 0 < properties & READ_AUTHENTICATED
        2
      elsif 0 < properties & READ_AUTHENTICATED_SC
        read_requires_sc = true
        2
      elsif 0 < properties & READ_ENCRYPTED
        1
      else
        0
      end
      write_security_level = if 0 < properties & WRITE_AUTHORIZED
        3
      elsif 0 < properties & WRITE_AUTHENTICATED
        2
      elsif 0 < properties & WRITE_AUTHENTICATED_SC
        write_requires_sc = true
        2
      elsif 0 < properties & WRITE_ENCRYPTED
        1
      else
        0
      end

      # map security requirements to flags
      properties |= READ_PERMISSION_BIT_1 if 0 < read_security_level & 2
      properties |= READ_PERMISSION_BIT_0 if 0 < read_security_level & 1
      properties |= READ_PERMISSION_SC if read_requires_sc
      properties |= WRITE_PERMISSION_BIT_1 if 0 < write_security_level & 2
      properties |= WRITE_PERMISSION_BIT_0 if 0 < write_security_level & 1
      properties |= WRITE_PERMISSION_SC if write_requires_sc

      return properties
    end

    def write_permissions_and_key_size_flags_from_properties(properties)
      att_flags(properties) & (ENCRYPTION_KEY_SIZE_MASK | WRITE_PERMISSION_BIT_0 | WRITE_PERMISSION_BIT_1)
    end

    def uuid2str(uuid)
      uuid.is_a?(String) ? uuid : Utils.int16_to_little_endian(uuid)
    end
  end

  class AdvertisingData
    def initialize
      @data = ""
    end

    attr_reader :data

    def add(type, *values)
      length_pos = @data.length
      @data << 0.chr # dummy length
      @data << type.chr
      length = 1
      values.each do |d|
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
      instance = self.new
      block.call(instance)
      adv_data = instance.data
      if 31 < adv_data.length
        raise ArgumentError, "too long AdvData. It must be less than 32 bytes."
      end
      adv_data
    end
  end
end

