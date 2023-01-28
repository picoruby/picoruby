class GPIO
  # This is a mimic of pico_error_codes
  # from pico-sdk/src/common/pico_base/include/pico/error.h
  ERROR_NONE = 0
  ERROR_TIMEOUT = -1
  ERROR_GENERIC = -2
  ERROR_NO_DATA = -3
  ERROR_NOT_PERMITTED = -4
  ERROR_INVALID_ARG = -5
  ERROR_IO = -6

  def handle_error(code, name = "")
    case code
    when ERROR_NONE
      return 0
    when ERROR_TIMEOUT
      raise(RuntimeError.new("Timeout error in #{name}"))
    else
      raise(RuntimeError.new("Unknown error in #{name}. code: #{code}"))
    end
  end

  def initialize(pin, *params)
    @pin = pin
    GPIO._init(pin)
    mode = disjunction(params)
    set_dir(mode)
    unless @dir
      raise ArgumentError.new("You must specify one of IN, OUT and HIGH_Z")
    end
    set_pull(mode, true)
    set_open_drain(mode, true)
  end

  def setmode(*params)
    mode = disjunction(params)
    set_dir(mode) if 0 < (mode & (IN|OUT|HIGH_Z))
    set_pull(mode)
    set_open_drain(mode)
  end

  # private

  def disjunction(params)
    mode = 0
    params.each { |param| mode |= param }
    mode
  end

  def set_dir(mode)
    mode_dir = ((mode & IN) + ((mode & OUT)>>1) + ((mode & HIGH_Z)>>2))
    case mode_dir
    when 0
      # do nothing
    when 1
      @dir = mode & (IN|OUT|HIGH_Z)
      GPIO._set_dir(@pin, @dir.to_i)
      return 0
    else
      raise ArgumentError.new("IN, OUT and HIGH_Z are exclusive")
    end
    0
  end

  def set_pull(mode, initialize = false)
    if initialize || 0 < (mode & (PULL_UP|PULL_DOWN))
      @pull = case mode & (PULL_UP|PULL_DOWN)
      when 0
        nil
      when PULL_UP, PULL_DOWN
        GPIO._set_pull(@pin, @pull.to_i)
        @pull
      else
        raise ArgumentError.new("PULL_UP and PULL_DOWN are exclusive")
      end
    end
    0
  end

  def set_open_drain(mode, initialize = false)
    if initialize || 0 < (mode & OPEN_DRAIN)
      @open_drain = (0 < (mode & 0b1000000))
      GPIO._set_open_drain(@pin) if @open_drain
      @open_drain
    end
    0
  end
end

