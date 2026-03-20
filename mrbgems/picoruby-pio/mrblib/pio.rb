module PIO
  PIO0 = 0
  PIO1 = 1

  FIFO_JOIN_NONE = 0
  FIFO_JOIN_TX   = 1
  FIFO_JOIN_RX   = 2

  # JMP conditions
  JMP_ALWAYS  = 0b000
  JMP_NOT_X   = 0b001
  JMP_X_DEC   = 0b010
  JMP_NOT_Y   = 0b011
  JMP_Y_DEC   = 0b100
  JMP_X_NE_Y  = 0b101
  JMP_PIN     = 0b110
  JMP_NOT_OSRE = 0b111

  JMP_CONDITIONS = {
    always:   JMP_ALWAYS,
    not_x:    JMP_NOT_X,
    x_dec:    JMP_X_DEC,
    not_y:    JMP_NOT_Y,
    y_dec:    JMP_Y_DEC,
    x_ne_y:   JMP_X_NE_Y,
    pin:      JMP_PIN,
    not_osre: JMP_NOT_OSRE
  }

  # WAIT sources
  WAIT_GPIO = 0b00
  WAIT_PIN  = 0b01
  WAIT_IRQ  = 0b10

  WAIT_SOURCES = {
    gpio: WAIT_GPIO,
    pin:  WAIT_PIN,
    irq:  WAIT_IRQ
  }

  # IN sources
  IN_PINS    = 0b000
  IN_X       = 0b001
  IN_Y       = 0b010
  IN_NULL    = 0b011
  IN_ISR     = 0b110
  IN_OSR     = 0b111

  IN_SOURCES = {
    pins: IN_PINS,
    x:    IN_X,
    y:    IN_Y,
    null: IN_NULL,
    isr:  IN_ISR,
    osr:  IN_OSR
  }

  # OUT destinations
  OUT_PINS    = 0b000
  OUT_X       = 0b001
  OUT_Y       = 0b010
  OUT_NULL    = 0b011
  OUT_PINDIRS = 0b100
  OUT_PC      = 0b101
  OUT_ISR     = 0b110
  OUT_EXEC    = 0b111

  OUT_DESTINATIONS = {
    pins:    OUT_PINS,
    x:       OUT_X,
    y:       OUT_Y,
    null:    OUT_NULL,
    pindirs: OUT_PINDIRS,
    pc:      OUT_PC,
    isr:     OUT_ISR,
    exec:    OUT_EXEC
  }

  # MOV destinations
  MOV_DEST_PINS = 0b000
  MOV_DEST_X    = 0b001
  MOV_DEST_Y    = 0b010
  MOV_DEST_EXEC = 0b100
  MOV_DEST_PC   = 0b101
  MOV_DEST_ISR  = 0b110
  MOV_DEST_OSR  = 0b111

  MOV_DESTINATIONS = {
    pins: MOV_DEST_PINS,
    x:    MOV_DEST_X,
    y:    MOV_DEST_Y,
    exec: MOV_DEST_EXEC,
    pc:   MOV_DEST_PC,
    isr:  MOV_DEST_ISR,
    osr:  MOV_DEST_OSR
  }

  # MOV sources
  MOV_SRC_PINS   = 0b000
  MOV_SRC_X      = 0b001
  MOV_SRC_Y      = 0b010
  MOV_SRC_NULL   = 0b011
  MOV_SRC_STATUS = 0b101
  MOV_SRC_ISR    = 0b110
  MOV_SRC_OSR    = 0b111

  MOV_SOURCES = {
    pins:   MOV_SRC_PINS,
    x:      MOV_SRC_X,
    y:      MOV_SRC_Y,
    null:   MOV_SRC_NULL,
    status: MOV_SRC_STATUS,
    isr:    MOV_SRC_ISR,
    osr:    MOV_SRC_OSR
  }

  # MOV operations
  MOV_OP_NONE    = 0b00
  MOV_OP_INVERT  = 0b01
  MOV_OP_REVERSE = 0b10

  MOV_OPS = {
    none:    MOV_OP_NONE,
    invert:  MOV_OP_INVERT,
    reverse: MOV_OP_REVERSE
  }

  # SET destinations
  SET_PINS    = 0b000
  SET_X       = 0b001
  SET_Y       = 0b010
  SET_PINDIRS = 0b100

  SET_DESTINATIONS = {
    pins:    SET_PINS,
    x:       SET_X,
    y:       SET_Y,
    pindirs: SET_PINDIRS
  }

  class Program
    attr_reader :instructions, :side_set_count, :side_set_optional
    attr_reader :wrap_target_idx, :wrap_idx

    def initialize(instructions, side_set_count, side_set_optional, wrap_target_idx, wrap_idx)
      @instructions = instructions
      @side_set_count = side_set_count
      @side_set_optional = side_set_optional
      @wrap_target_idx = wrap_target_idx
      @wrap_idx = wrap_idx
    end

    def length
      @instructions.length
    end
  end

  class Assembler
    def initialize(side_set: 0, side_set_optional: false)
      @side_set_count = side_set
      @side_set_optional = side_set_optional
      @instructions = []
      @labels = {}
      @pending_labels = []
      @wrap_target_idx = 0
      @wrap_idx = nil
      @_jmp_targets = []
    end

    def label(name)
      @labels[name] = @instructions.length
      @pending_labels << name
    end

    def wrap_target
      @wrap_target_idx = @instructions.length
    end

    def wrap
      @wrap_idx = @instructions.length - 1
    end

    # --- Instructions ---

    def jmp(addr, cond: :always, side: nil, delay: 0)
      cond = JMP_CONDITIONS[cond]
      raise ArgumentError, "Unknown jmp condition: #{cond}" if cond.nil?
      instr = (0b000 << 13) | (cond << 5) | 0 # addr filled in resolve
      @instructions << apply_side_delay(instr, side, delay)
      mark_jmp_target(addr, @instructions.length - 1)
    end

    def wait(polarity, source, index, side: nil, delay: 0)
      src = WAIT_SOURCES[source]
      raise ArgumentError, "Unknown wait source: #{source}" if src.nil?
      instr = (0b001 << 13) | ((polarity & 1) << 7) | (src << 5) | (index & 0x1F)
      @instructions << apply_side_delay(instr, side, delay)
    end

    # `in` is a Ruby reserved word, so use `in_`
    def in_(source, bit_count, side: nil, delay: 0)
      src = IN_SOURCES[source]
      raise ArgumentError, "Unknown in source: #{source}" if src.nil?
      bc = bit_count == 32 ? 0 : (bit_count & 0x1F)
      instr = (0b010 << 13) | (src << 5) | bc
      @instructions << apply_side_delay(instr, side, delay)
    end

    def out(dest, bit_count, side: nil, delay: 0)
      dst = OUT_DESTINATIONS[dest]
      raise ArgumentError, "Unknown out destination: #{dest}" if dst.nil?
      bc = bit_count == 32 ? 0 : (bit_count & 0x1F)
      instr = (0b011 << 13) | (dst << 5) | bc
      @instructions << apply_side_delay(instr, side, delay)
    end

    def push(iffull: false, block: true, side: nil, delay: 0)
      instr = (0b100 << 13) | ((iffull ? 1 : 0) << 6) | ((block ? 1 : 0) << 5)
      @instructions << apply_side_delay(instr, side, delay)
    end

    def pull(ifempty: false, block: true, side: nil, delay: 0)
      instr = (0b100 << 13) | (1 << 7) | ((ifempty ? 1 : 0) << 6) | ((block ? 1 : 0) << 5)
      @instructions << apply_side_delay(instr, side, delay)
    end

    def mov(dest, source, op: :none, side: nil, delay: 0)
      dst = MOV_DESTINATIONS[dest]
      raise ArgumentError, "Unknown mov destination: #{dest}" if dst.nil?
      src = MOV_SOURCES[source]
      raise ArgumentError, "Unknown mov source: #{source}" if src.nil?
      operation = MOV_OPS[op]
      raise ArgumentError, "Unknown mov op: #{op}" if operation.nil?
      instr = (0b101 << 13) | (dst << 5) | (operation << 3) | src
      @instructions << apply_side_delay(instr, side, delay)
    end

    def irq(index, wait: false, clear: false, relative: false, side: nil, delay: 0)
      idx = index & 0x07
      idx |= 0x10 if relative
      instr = (0b110 << 13) | ((clear ? 1 : 0) << 6) | ((wait ? 1 : 0) << 5) | idx
      @instructions << apply_side_delay(instr, side, delay)
    end

    def set(dest, value, side: nil, delay: 0)
      dst = SET_DESTINATIONS[dest]
      raise ArgumentError, "Unknown set destination: #{dest}" if dst.nil?
      instr = (0b111 << 13) | (dst << 5) | (value & 0x1F)
      @instructions << apply_side_delay(instr, side, delay)
    end

    def nop(side: nil, delay: 0)
      # NOP is encoded as MOV Y, Y
      mov(:y, :y, side: side, delay: delay)
    end

    def assemble
      resolve_labels
      # @type var wrap_idx: Integer
      wrap_idx = @wrap_idx.nil? ? @instructions.length - 1 : @wrap_idx
      Program.new(@instructions.dup, @side_set_count, @side_set_optional, @wrap_target_idx, wrap_idx)
    end

    private

    def delay_side_bits
      # Total bits for side-set + delay = 5
      # If side_set_optional, 1 bit is used for the optional flag
      if @side_set_optional
        @side_set_count + 1
      else
        @side_set_count
      end
    end

    def max_delay
      (1 << (5 - delay_side_bits)) - 1
    end

    def apply_side_delay(instr, side, delay)
      side_bits = delay_side_bits
      delay_bits = 5 - side_bits

      if max_delay < delay
        raise ArgumentError, "Delay #{delay} exceeds max #{max_delay}"
      end

      side_delay = 0
      if side.nil?
        if 0 < @side_set_count && !@side_set_optional
          raise ArgumentError, "side value required when side_set is not optional"
        end
        side_delay = delay & ((1 << delay_bits) - 1)
      else
        if @side_set_optional
          side_delay = (1 << (side_bits - 1 + delay_bits)) |
                       ((side & ((1 << @side_set_count) - 1)) << delay_bits) |
                       (delay & ((1 << delay_bits) - 1))
        else
          side_delay = ((side & ((1 << @side_set_count) - 1)) << delay_bits) |
                       (delay & ((1 << delay_bits) - 1))
        end
      end

      instr | (side_delay << 8)
    end

    def mark_jmp_target(addr, instr_idx)
      @_jmp_targets << [instr_idx, addr]
    end

    def resolve_labels
      @_jmp_targets.each do |instr_idx, addr|
        if addr.is_a?(Symbol)
          target = @labels[addr]
          raise ArgumentError, "Undefined label: #{addr}" if target.nil?
        else
          target = addr
        end
        @instructions[instr_idx] = (@instructions[instr_idx] & 0xFFE0) | (target & 0x1F)
      end
    end
  end

  def self.asm(side_set: 0, side_set_optional: false, &block)
    assembler = Assembler.new(side_set: side_set, side_set_optional: side_set_optional)
    assembler.instance_eval(&block)
    assembler.assemble
  end
end
