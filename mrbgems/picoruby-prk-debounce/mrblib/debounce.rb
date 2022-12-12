# DebounceBase is meant to be inherited by a child class.
# So you can't use it directly.
class DebounceBase
  DEFAULT_THRESHOLD = 40

  def initialize
    puts "Init #{self.class}"
    @threshold = DEFAULT_THRESHOLD
    @now = 0
  end

  def threshold=(val)
    if !val.is_a?(Integer) || val < 1
      puts "Invalid Debounce threshold: #{val}"
      @threshold = DEFAULT_THRESHOLD
    else
      @threshold = val
    end
    puts "Debounce threshold: #{@threshold}"
  end

  def set_time
    @now = board_millis
  end
end

# If you are confident your switches never bounce mechanically
# and you are a PC game enthusiast, use DebounceNone which
# doesn't cancel any bounce and performs maximum.
# - Usage: `kbd.set_debounce(:none)`
class DebounceNone < DebounceBase
  def set_time
  end
  def resolve(in_pin, _out_pin)
    gpio_get(in_pin)
  end
end

# DebouncePerRow
# You don't need to explicitly configure it
# because it'll be appended by default
# - Algorithm: Symmetric eager debounce per row
# - RAM consumption: Smaller than DebouncePerKey
# - Usage: `kbd.set_debounce(:per_row)`
class DebouncePerRow < DebounceBase
  def initialize
    @pr_table = {}
    super
  end

  def resolve(in_pin, out_pin)
    pin_val = gpio_get(in_pin)
    status = @pr_table[out_pin]
    unless status
      @pr_table[out_pin] = { in_pin: nil, pin_val: pin_val, time: @now }
      return pin_val
    end
    if pin_val
      unless status[:pin_val] || status[:in_pin] != in_pin
        if status[:in_pin] == in_pin && @now - status[:time] < @threshold
          return false
        else
          @pr_table[out_pin] = { in_pin: in_pin, pin_val: true, time: @now }
          return true
        end
      else
        return true
      end
    else
      if status[:pin_val]
        if status[:in_pin] == in_pin && @now - status[:time] < @threshold
          return true
        else
          @pr_table[out_pin] = { in_pin: in_pin, pin_val: false, time: @now }
          return false
        end
      else
        return false
      end
    end
  end
end

# DebouncePerKey
# The most recommended one unless out of memory
# - Algorithm: Symmetric eager debounce per key
# - RAM consumption: Bigger than DebouncePerRow
# - Usage: `kbd.set_debounce(:per_key)`
class DebouncePerKey < DebounceBase
  def initialize
    @pk_table = {}
    super
  end

  def resolve(in_pin, out_pin)
    pin_val = gpio_get(in_pin)
    key = in_pin << 8 | out_pin
    status = @pk_table[key]
    unless status
      @pk_table[key] = { pin_val: pin_val, time: @now }
      return pin_val
    end
    if status[:pin_val] != pin_val
      if @now - status[:time] < @threshold
        !pin_val
      else
        @pk_table[key] = { pin_val: pin_val, time: @now }
        pin_val
      end
    else
      pin_val
    end
  end
end
