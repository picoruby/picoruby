class BLE
  class GattDatabase
    ATT_DB_VERSION = 0x01

    def initialize(&block)
      @profile_data = ATT_DB_VERSION.chr
      @handle_table = {}
      @current_handle = 0
      @hash_src = ""
      @hash_pos = nil
      block.call self
      insert_database_hash if @database_hash_key
      @profile_data << "\x00\x00"
    end

    attr_reader :handle_table, :profile_data

    def add_service(uuid, service_uuid)
      if uuid != GATT_PRIMARY_SERVICE_UUID && uuid != GATT_SECONDARY_SERVICE_UUID
        raise "invalid uuid: #{uuid.inspect}"
      end
      line = Utils.int16_to_little_endian(READ)
      tail = [ push_handle, uuid2str(uuid), uuid2str(service_uuid) ].join
      line << tail
      @hash_src << tail
      add_line(line)
      @handle_table[service_uuid] = { handle: @current_handle }
      @service_uuid = service_uuid
      yield self if block_given?
      @service_uuid = nil
    end

    def add_characteristic(properties, char_uuid, value_properties, value)
      # declaration
      line = Utils.int16_to_little_endian(READ)
      tail = [ push_handle,
        Utils.int16_to_little_endian(GATT_CHARACTERISTIC_UUID),
        (properties & 0xff).chr,
        seek_handle,
        uuid2str(char_uuid) ].join
      line << tail
      @hash_src << tail
      add_line(line)
      @handle_table[@service_uuid][char_uuid] = { handle: @current_handle, value_handle: nil }
      # value
      if char_uuid == CHARACTERISTIC_DATABASE_HASH
        if !value.is_a?(String) || value.length != 16
          raise "database hash key must be 16 bytes string: #{value.inspect}"
        end
        @database_hash_key = value
        @hash_pos = @profile_data.length + line.length - 3
      end
      if value
        line = flag_by_uuid(char_uuid, value_properties)
        line << push_handle << uuid2str(char_uuid) << value
        add_line(line)
        @handle_table[@service_uuid][char_uuid] = { value_handle: @current_handle }
      end
      @char_uuid = char_uuid
      yield self if block_given?
      @char_uuid = nil
    end

    def add_descriptor(properties, desc_uuid, value = "")
      line = flag_by_uuid(desc_uuid, properties)
      tail = [ push_handle, uuid2str(desc_uuid), value ].join
      line << tail
      @hash_src << tail
      add_line(line)
      @handle_table[@service_uuid][@char_uuid][desc_uuid] = @current_handle
    end

    # private

    def insert_database_hash
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
      Utils.int16_to_little_endian(@current_handle)
    end

    def seek_handle
      Utils.int16_to_little_endian(@current_handle + 1)
    end

    def add_line(line)
      @profile_data << Utils.int16_to_little_endian(line.length + 2) << line
    end

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

    def flag_by_uuid(uuid, properties)
      if properties & (NOTIFY | INDICATE) != 0
        # characteristic client configuration
        flag = write_permissions_and_key_size_flags_from_properties(properties)
        flag |= READ
        flag |= WRITE
        flag |= WRITE_WITHOUT_RESPONSE
        flag |= DYNAMIC
      else
        flag = properties
      end
      if uuid.is_a?(String) && uuid.length == 16
        Utils.int16_to_little_endian(flag|LONG_UUID)
      elsif uuid.is_a?(Integer)
        Utils.int16_to_little_endian(flag)
      else
        raise "invalid uuid: #{uuid.inspect}"
      end
    end

    def uuid2str(uuid)
      if uuid.is_a?(String)
        if uuid.length == 16
          uuid
        else
          raise "invalid uuid: #{uuid.inspect}"
        end
      else
        # TODO: Fix pico-compiler to support "unsigned" 32bit integer
        if uuid < 0 #|| 0xffffffff < uuid
          raise "invalid uuid: #{uuid.inspect}"
        elsif uuid < 0x10000
          Utils.int16_to_little_endian(uuid)
        #elsif uuid < 0x100000000
        else
          Utils.int32_to_little_endian(uuid)
        #else
        #  raise "invalid uuid: #{uuid.inspect}"
        end
      end
    end

  end
end
