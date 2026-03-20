require 'irq'

# Quadrature encoder using picoruby-irq

class RotaryEncoder
  def initialize(pin_a, pin_b)
    @gpio_a   = GPIO.new(pin_a, GPIO::IN | GPIO::PULL_UP)
    @gpio_b   = GPIO.new(pin_b, GPIO::IN | GPIO::PULL_UP)
    @status   = 0
    @proc_cw  = Proc.new {}
    @proc_ccw = @proc_cw

    edge = GPIO::EDGE_FALL | GPIO::EDGE_RISE

    # capture: self because mruby/c cannot close over outer locals in callbacks
    @gpio_a.irq(edge, capture: self) do |_gpio, event_type, encoder|
      # @type var encoder: RotaryEncoder
      encoder.on_pin_a(event_type)
    end
    @gpio_b.irq(edge, capture: self) do |_gpio, event_type, encoder|
      # @type var encoder: RotaryEncoder
      encoder.on_pin_b(event_type)
    end
  end

  # Public for capture-based dispatch
  def on_pin_a(event_type)
    read_and_decode
  end

  # Public for capture-based dispatch
  def on_pin_b(event_type)
    read_and_decode
  end

  def clockwise(&block)
    @proc_cw = block
  end
  alias cw clockwise

  def counterclockwise(&block)
    @proc_ccw = block
  end
  alias ccw counterclockwise

  private

  # @status: 0b111111
  #                ^^ current status
  #              ^^   previous status
  #            ^^     possible rotate direction
  #
  #        ccw <-|-> cw
  # pin_a: 1 1 0 0 1 1 0
  # pin_b: 0 1 1 0 0 1 1
  #             (^ start position)
  #
  # current_status encodes active-low pin readings:
  #   bit 1 = pin_a active (LOW)
  #   bit 0 = pin_b active (LOW)
  #
  # Rotation detection (rotation stored pre-shifted in bits [5:4] of @status):
  #   (A) If rotation==0 and current transitions to 0b01 or 0b10,
  #       record the possible direction (stored as current<<4).
  #   (B) When current returns to 0b00 (both at rest):
  #       rotation==0b100000 && prev==0b01 => CW  => @proc_cw.call
  #       rotation==0b010000 && prev==0b10 => CCW => @proc_ccw.call
  # Read both pins directly (like the C version) for accurate state
  def read_and_decode
    current  = (@gpio_a.low? ? 0b10 : 0) | (@gpio_b.low? ? 0b01 : 0)
    prev     = @status & 0b000011
    rotation = @status & 0b110000
    if current != prev
      if rotation == 0b000000
        rotation = current << 4 if current == 0b01 || current == 0b10
      elsif current == 0b00
        if rotation == 0b100000 && prev == 0b01
          @proc_cw.call
        elsif rotation == 0b010000 && prev == 0b10
          @proc_ccw.call
        end
        rotation = 0b000000
      end
      @status = rotation | (prev << 2) | current
    end
  end
end
