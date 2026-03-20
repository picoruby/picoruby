module MbedTLS
  module PKey
    class EC < MbedTLS::PKey::PKeyBase
      def free
        nil
      end
    end
  end
end
