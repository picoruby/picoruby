class MbedTLS
  class CMAC
    def self.new(key, digest)
      unless key.is_a?(String)
        raise ArgumentError, "Invalid key type: #{key.class}"
      end
      case digest
      when "AES", "aes"
        unless key.length == 16
          raise ArgumentError, "Invalid key length: #{key.length}"
        end
        instance = self._init_aes(key)
        instance._digest = "aes"
        return instance
      else
        raise ArgumentError, "Unsupported digest: #{digest}"
      end
    end

    attr_accessor :_digest
  end
end

