class MbedTLS
  class Digest
    SUPPORTED_ALGORITHMS = {
      :sha256 => 0x01
    }

    def self.new(algorithm)
      unless SUPPORTED_ALGORITHMS[algorithm] != nil
        raise ArgumentError, "Algorithm not supported: #{algorithm}"
      end
      instance = self._init_ctx(SUPPORTED_ALGORITHMS[algorithm])

      return instance
    end
  end
end
