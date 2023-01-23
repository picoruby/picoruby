class GPIO
  # This is a mimic of pico_error_codes
  # from pico-sdk/src/common/pico_base/include/pico/error.h
  ERROR_TIMEOUT = -1
  ERROR_GENERIC = -2
  ERROR_NO_DATA = -3
  ERROR_NOT_PERMITTED = -4
  ERROR_INVALID_ARG = -5
  ERROR_IO = -6

  def handle_error(code, name = "")
    case code
    when ERROR_TIMEOUT
      raise(RuntimeError.new("Timeout error in #{name}"))
    else
      raise(RuntimeError.new("Unknown error in #{name}. code: #{code}"))
    end
    code
  end
end

