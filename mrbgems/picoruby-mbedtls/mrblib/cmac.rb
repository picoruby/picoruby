module MbedTLS
  class CMAC
    def self.new(key, digest)
      unless key.is_a?(String)
        raise ArgumentError, "Invalid key type: #{key.class}"
      end
      case digest.downcase
      when "aes"
        unless key.length == 16
          raise ArgumentError, "Invalid key length: `#{key.inspect}`"
        end
        instance = _init_aes(key)
        instance._digest = "aes"
        return instance
      else
        raise ArgumentError, "Unsupported digest: #{digest}"
      end
    end

    attr_accessor :_digest
  end
end

