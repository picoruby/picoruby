class RGB
  KEYCODE = {
    RGB_TOG:          0x601,
    RGB_MODE_FORWARD: 0x602,
    RGB_MOD:          0x603,
    RGB_MODE_REVERSE: 0x604,
    RGB_RMOD:         0x605,
    RGB_HUI:          0x606,
    RGB_HUD:          0x607,
    RGB_SAI:          0x608,
    RGB_SAD:          0x609,
    RGB_VAI:          0x60a,
    RGB_VAD:          0x60b,
    RGB_SPI:          0x60c,
    RGB_SPD:          0x60d
    #                 ^^^^^
    #                 Hard-coded values
  }

 def initialize(pin, underglow_size, backlight_size, is_rgbw = false)
    puts "Init RGB"
    turn_off
    @fifo = Array.new
    # TODO: @underglow_size, @backlight_size
    @pixel_size = underglow_size + backlight_size
    ws2812_init(pin, @pixel_size, is_rgbw)
    @max_value = 13 # default
    init_values
  end

  attr_reader :pixel_size, :effect
  attr_accessor :action, :anchor

  def init_values
    self.speed = @speed || 25
    @hue = 0
    @saturation = 100
    @value = 0.0
  end

  def hsv2rgb(h, s, v)
    s /= 100.0
    v /= 100.0
    c = v * s
    x = c * (1 - ((h / 60.0).modulo(2) - 1).abs)
    m = v - c
    rgb = if h < 60
            [c, x, 0]
          elsif h < 120
            [x, c, 0]
          elsif h < 180
            [0, c, x]
          elsif h < 240
            [0, x, c]
          elsif h < 300
            [x, 0, c]
          else
            [c, 0, x]
          end
    ((rgb[0] + m) * 255).ceil_to_i << 16 |
      ((rgb[1] + m) * 255).ceil_to_i << 8 |
      ((rgb[2] + m) * 255).ceil_to_i
  end

  EFFECTS = %i|swirl rainbow_mood breath nokogiri static circle|

  def effect=(name)
    unless EFFECTS.include?(name)
      puts "Error: Unknown RGB effect `:#{name}`"
      return
    end
    turn_off
    @effect = name
    @effect_init = false
    init_values
    reset_pixel
    turn_on
  end

  def reset_pixel
    case @effect
    when :swirl
      step = 5
      i = 0
      (360 / step).times do
        ws2812_set_pixel_at(i, hsv2rgb(i * step, @saturation, @max_value))
        i += 1
      end
      ws2812_set_pixel_at(i, -1)
    when :rainbow
      puts "[WARN] :rainbow is deprecated. Use :swirl instead"
    when :breathing
      puts "[WARN] :breathing is deprecated. Use :rainbow_mood instead"
    end
  end

  def turn_on
    @offed = false
  end

  def turn_off
    @offed = true
  end

  def show
    if @offed
      ws2812_fill(0, @pixel_size)
      sleep 1
      return
    end
    unless @fifo.empty?
      case @action
      when :thunder
        thunder
      end
    end
    case @effect
    when :swirl
      @ping = true if ws2812_rotate_swirl(@pixel_size)
    when :rainbow_mood
      if 360 <= @hue
        @ping = true
        @hue = 0
      else
        @hue += 6
      end
      ws2812_fill(hsv2rgb(@hue, @saturation, @max_value), @pixel_size)
    when :breath
      if @ascent
        @value < @max_value ? @value += (@max_value / 31.0) : @ascent = false
      else
        if 0 < @value
          @value -= (@max_value / 31.0)
          @value = 0.0 if @value < 0
        else
          @ping = true
          @value = 0.0
          @ascent = true
        end
      end
      ws2812_fill(hsv2rgb(@hue, @saturation, @value), @pixel_size)
    when :ruby, :nokogiri
      if @max_value <= @value
        @ping = true
        @value = 0.0
      else
        @value += (@max_value / 31.0)
      end
      ws2812_fill(hsv2rgb(@hue, @saturation, @value), @pixel_size)
    when :static
      ws2812_fill(hsv2rgb(@hue, @saturation, @max_value), @pixel_size)
    when :circle
      unless @effect_init
        ws2812_init_pixel_distance(@pixel_size)
        @circle_diameter = 0
        @effect_init = true
      end
      @circle_diameter -= 1
      if @circle_diameter < 0
        @ping = true
        @circle_diameter = 11
      end
      ws2812_circle(@pixel_size, @max_value, @circle_diameter)
    end
    sleep_ms @delay
  end

  def ping?
    if @ping
      @ping = false
      return true
    else
      return false
    end
  end

  def toggle
    if @offed
      reset_pixel
      puts "On"
    else
      turn_off
      puts "Off"
    end
  end

  def hue=(val)
    @hue = val
    @hue = 348 if @hue < 0
    @hue = 0 if 348 < @hue
    reset_pixel
  end

  def saturation=(val)
    @saturation = val
    @saturation = 0 if @saturation < 0
    @saturation = 100 if 100 < @saturation
    reset_pixel
  end

  def value=(val)
    @max_value = val
    @max_value = 0 if @max_value < 0
    @max_value = 31 if 31 < @max_value
    @value = [@value, @max_value].min.to_f
    reset_pixel
  end

  def speed=(val)
    @speed = val
    @speed = 0 if @speed < 0
    @speed = 31 if 31 < @speed
    @delay = (33 - @speed) * 10
    unless @anchor
      # speed tuning (partner is slower due to blocking UART)
      @delay = (@delay * 0.97).ceil_to_i
    end
  end

  def invoke_anchor(key)
    message = 0
    if @last_key == key # preventing double invoke
      @last_key = nil
      return message
    end
    @last_key = key
    print "#{key} / "
    case key
    when :RGB_TOG, :RGB_MODE_FORWARD, :RGB_MOD, :RGB_MODE_REVERSE, :RGB_RMOD
      message = 0b00100000 # 1 << 5
      if key == :RGB_TOG
        toggle
        message += 1 unless @offed
      else
        effect_index = EFFECTS.index(@effect).to_i
        name = if key == :RGB_MODE_FORWARD || key == :RGB_MOD
          EFFECTS[effect_index + 1] || EFFECTS.first
        else
          effect_index == 0 ? EFFECTS.last : EFFECTS[effect_index - 1]
        end
        self.effect = name || :swirl
        message += EFFECTS.index(@effect).to_i + 2
        puts "effect: #{@effect}"
      end
    when :RGB_HUI, :RGB_HUD
      message = 0b01000000 # 2 << 5
      self.hue = key == :RGB_HUI ? @hue + 12 : @hue - 12
      puts "hue: #{@hue}"
      message |= (@hue / 12)
    when :RGB_SAI, :RGB_SAD
      message = 0b01100000 # 3 << 5
      self.saturation = key == :RGB_SAI ? @saturation + 5 : @saturation - 5
      puts "saturation: #{@saturation}"
      message |= (@saturation / 5)
    when :RGB_VAI, :RGB_VAD
      message = 0b10000000 # 4 << 5
      self.value = key == :RGB_VAI ? @max_value + 1 : @max_value - 1
      puts "value: #{@max_value}"
      message |= @max_value
    when :RGB_SPI, :RGB_SPD
      message = 0b10100000 # 5 << 5
      self.speed = key == :RGB_SPI ? @speed + 1 : @speed - 1
      puts "speed: #{@speed}"
      message |= @speed
    else
      puts "Error: Unknown method"
    end
    sleep 0.2
    return message
  end
  #            ...      method
  # message: 0b11111111
  #               ^^^^^ value
  def invoke_partner(message)
    val = message & 0b00011111
    case message >> 5
    when 1
      if val <= 1
        @offed = (val == 1)
        toggle
      else
        self.effect = EFFECTS[val - 2]
      end
    when 2
      self.hue = val * 12
    when 3
      self.saturation = val * 5
    when 4
      self.value = val
    when 5
      self.speed = val
    when 7 # ping
      case @effect
      when :swirl
        ws2812_reset_swirl_index
      when :rainbow_mood
        @hue = 0
      when :breath, :nokogiri
        @ascent = true
        @value = 0.0
      when :circle
        @circle_diameter = 11
      end
    when 0
      puts "ERROR: This should not happen"
    end
  end

  def fifo_push(data)
    return if @fifo.size > 2
    @fifo << data
  end

  def thunder
    unless @srand
      # generate seed with board_millis
      srand(board_millis)
      @srand = true
    end
    4.times do |salt|
      ws2812_rand_show(0x202020, (salt+1) * 2, @pixel_size)
      sleep_ms 3
    end
    @fifo.shift
  end

  def add_pixel(x, y)
    @pixel_index ||= 0
    # @type ivar @pixel_index: Integer
    if 150 <= @pixel_index
      puts "Error: MAX_PIXEL_SIZE reached"
      return
    end
    ws2812_add_matrix_pixel_at(@pixel_index, x, y)
    @pixel_index += 1
  end

end
