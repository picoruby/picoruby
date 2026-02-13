module DRb
  class DRbObject
    def initialize(uri, ref = nil)
      @uri = uri
      @ref = ref
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
      h = 0
      @uri.each_byte { |b| h = h * 31 + b }
      if @ref.is_a?(String)
        @ref.each_byte { |b| h = h * 31 + b }
      elsif @ref.is_a?(Symbol)
        @ref.to_s.each_byte { |b| h = h * 31 + b }
      elsif @ref.is_a?(Integer)
        h = h * 31 + @ref
      else
        h = h * 31 + @ref.object_id
      end
      h
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
