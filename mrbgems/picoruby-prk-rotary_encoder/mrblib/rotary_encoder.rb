class RotaryEncoder
 def initialize(pin_a, pin_b)
    puts "Init RotaryEncoder"
    @pin_a = pin_a
    @pin_b = pin_b
    @partner_keycode_cw = 0
    @partner_keycode_ccw = 0
    @split_left = true
    @proc_cw = Proc.new { }
    @proc_ccw = @proc_cw
  end

  def init_pins
    @number = init_encoder(@pin_a, @pin_b)
  end

  def create_keycodes(encoders_size)
    #                     0b11111111 is reserved
    @partner_keycode_cw = 0b11111110 - encoders_size * 2
    @partner_keycode_ccw = @partner_keycode_cw - 1
  end

  def left?
    @split_left
  end

  def right?
    !@split_left
  end

  def configure(symbol)
    case symbol
    when :left
      @split_left = true
    when :right
      @split_left = false
    end
  end

  attr_accessor :split_left

  def clockwise(&block)
    @proc_cw = block
  end

  def counterclockwise(&block)
    @proc_ccw = block
  end

  def call_cw
    @proc_cw.call
  end
  def call_ccw
    @proc_ccw.call
  end
  def call_proc_if(keycode)
    case keycode
    when @partner_keycode_cw
      @proc_cw.call
    when @partner_keycode_ccw
      @proc_ccw.call
    end
  end

  def consume_rotation_anchor
    case consume_encoder(@number)
    when 1
      @proc_cw.call
    when -1
      @proc_ccw.call
    end
  end

  def consume_rotation_partner
    case consume_encoder(@number)
    when 1
      @partner_keycode_cw
    when -1
      @partner_keycode_ccw
    else
      0
    end
  end
end
