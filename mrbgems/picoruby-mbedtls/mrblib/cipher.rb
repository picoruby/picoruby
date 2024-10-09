module MbedTLS
  class Cipher
    SUPPORTED_CIPHERS = {
      aes_128_cbc: 0x0001,
      aes_192_cbc: 0x0002,
      aes_256_cbc: 0x0003,
      aes_128_gcm: 0x1001,
      aes_192_gcm: 0x1002,
      aes_256_gcm: 0x1003
    }
    KEY_LENGTHS = {
      aes_128_cbc: 16,
      aes_192_cbc: 24,
      aes_256_cbc: 32,
      aes_128_gcm: 16,
      aes_192_gcm: 24,
      aes_256_gcm: 32
    }
    IV_LENGTHS = {
      0x0001 => 16,
      0x0002 => 16,
      0x0003 => 16,
      0x1001 => 12,
      0x1002 => 12,
      0x1003 => 12
    }
    SUPPORTED_OPERATIONS = {
      encrypt: 0,
      decrypt: 1
    }

    def self.new(cipher_suite, key, operation)
      unless SUPPORTED_CIPHERS[cipher_suite] != nil
        raise ArgumentError, "Cipher suite not supported: #{cipher_suite}"
      end
      unless SUPPORTED_OPERATIONS[operation] != nil
        raise ArgumentError, "Operation #{operation} not supported"
      end
      unless key.is_a?(String)
        raise ArgumentError, "Invalid key type: #{key.class}"
      end
      unless key.length == KEY_LENGTHS[cipher_suite]
        raise ArgumentError, "Invalid key length: `#{key.inspect}`"
      end
      instance = self._init_ctx(SUPPORTED_CIPHERS[cipher_suite], key, SUPPORTED_OPERATIONS[operation])

      return instance
    end

    def set_iv(iv)
      if iv.length != IV_LENGTHS[@cipher_key]
        raise ArgumentError, "Invalid IV length: #{iv.length}"
      end
      unless @iv_set
        @iv_set = true
        _set_iv(iv)
      else
        puts "[WARN] set_iv should be called once per instance, ignoring"
        self
      end
    end

  end
end

