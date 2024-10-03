module MbedTLS
  class HMAC
    def self.new(key, digest)
      unless key.is_a?(String)
        raise ArgumentError, "Invalid key type: #{key.class}"
      end
      case digest.downcase
      when "sha256"
        instance = self._init_sha256(key)
        instance._digest = "sha256"
        return instance
      else
        raise ArgumentError, "Unsupported digest: #{digest}"
      end
    end

    attr_accessor :_digest
  end
end
