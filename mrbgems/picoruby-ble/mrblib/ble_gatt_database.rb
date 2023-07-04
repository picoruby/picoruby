class BLE
  class GattDatabase
    ATT_DB_VERSION = 0x01
    def initialize(&block)
      @profile_data = ATT_DB_VERSION.chr
      @handle_table = {
        service: {},
        characteristic: {
          value: {},
          client_configuration: {},
        }
      }
      @current_handle = 0
      @hash_src = ""
      @hash_pos = nil
      @database_hash_key = "\x00" * 16
      block.call self
      insert_database_hash
      @profile_data << "\x00\x00"
    end

    attr_reader :handle_table
    attr_reader :profile_data
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
      @current_handle += 1
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
      @handle_table[:service][uuid] = @current_handle
      yield self if block_given?
    end

    def add_characteristic(uuid, properties, *values)
      # declaration
      line = Utils.int16_to_little_endian(READ)
      [
        Utils.int16_to_little_endian(push_handle),
        Utils.int16_to_little_endian(GATT_CHARACTERISTIC_UUID),
        (properties & 0xff).chr,
        Utils.int16_to_little_endian(@current_handle + 1),
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
      @handle_table[:characteristic][:value][uuid] = @current_handle
      if properties & (NOTIFY | INDICATE) != 0
        # characteristic client configuration
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
        @handle_table[:characteristic][:client_configuration][uuid] = @current_handle
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
end
