class BLE
  class Peripheral < BLE
    def initialize(profile_data, debug = false)
      @debug = debug
      @_read_values = {}
      @_write_values = {}
      CYW43.init
      _init(profile_data)
    end

    def get_write_value(handle)
      @_write_values.delete(handle)
    end

    def set_read_value(handle, value)
      # @type var handle: untyped
      unless handle.is_a?(Integer)
        raise TypeError, "handle must be Integer"
      end
      # @type var value: untyped
      unless value.is_a?(String)
        raise TypeError, "value must be String"
      end
      @_read_values[handle] = value
    end
  end
end
