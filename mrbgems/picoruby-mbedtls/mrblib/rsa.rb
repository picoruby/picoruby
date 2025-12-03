module MbedTLS
  module PKey
    class PKeyBase
    end
    class RSA < MbedTLS::PKey::PKeyBase
      def free
        # No-op for backward compatibility
        # https://github.com/picoruby/picoruby/pull/302
        nil
      end
    end
  end
end
