module DRb
  class DRbObject
    def initialize(uri, ref = nil)
      @uri = uri
      @ref = ref || self
    end

    attr_reader :uri, :ref

    def method_missing(msg_id, *args, &block)
      DRb.send_message(@uri, @ref, msg_id, args, block)
    end

    def respond_to_missing?(msg_id, include_private = false)
      true
    end

    def ==(other)
      return false unless other.is_a?(DRbObject)
      @uri == other.uri && @ref == other.ref
    end

    def eql?(other)
      self == other
    end

    def hash
      [@uri, @ref].hash
    end
  end

  class DRbUnknown
    def initialize(err, buf)
      @err = err
      @buf = buf
    end

    attr_reader :err, :buf

    def exception
      DRbUnknownError.new(@err, @buf)
    end
  end
end
