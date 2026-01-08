module MbedTLS
  class Digest
    def free
      # No-op for backward compatibility
      # https://github.com/picoruby/picoruby/pull/302
      nil
    end
  end
end
