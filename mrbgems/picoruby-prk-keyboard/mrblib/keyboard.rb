if RUBY_ENGINE == 'mruby/c'
  require "debounce"
  require "rgb"
  require "rotary_encoder"
  require "task-ext"
  require "vfs"
  require "filesystem-fat"
end

class Keyboard
  GPIO_IN          = 0b000
  GPIO_IN_PULLUP   = 0b010
  GPIO_IN_PULLDOWN = 0b110
  GPIO_OUT         = 0b001
  GPIO_OUT_LO      = 0b011
  GPIO_OUT_HI      = 0b101

  LED_NUMLOCK    = 0b00001
  LED_CAPSLOCK   = 0b00010
  LED_SCROLLLOCK = 0b00100
  LED_COMPOSE    = 0b01000
  LED_KANA       = 0b10000

  MOD_KEYCODE = {
    KC_LCTL: 0b00000001,
    KC_LSFT: 0b00000010,
    KC_LALT: 0b00000100,
    KC_LGUI: 0b00001000,
    KC_RCTL: 0b00010000,
    KC_RSFT: 0b00100000,
    KC_RALT: 0b01000000,
    KC_RGUI: 0b10000000
  }

  KEYCODE = [
    :KC_NO,               # 0x00
    :KC_ROLL_OVER,
    :KC_POST_FAIL,
    :KC_UNDEFINED,
    :KC_A,
    :KC_B,
    :KC_C,
    :KC_D,
    :KC_E,
    :KC_F,
    :KC_G,
    :KC_H,
    :KC_I,
    :KC_J,
    :KC_K,
    :KC_L,
    :KC_M,                # 0x10
    :KC_N,
    :KC_O,
    :KC_P,
    :KC_Q,
    :KC_R,
    :KC_S,
    :KC_T,
    :KC_U,
    :KC_V,
    :KC_W,
    :KC_X,
    :KC_Y,
    :KC_Z,
    :KC_1,
    :KC_2,
    :KC_3,                # 0x20
    :KC_4,
    :KC_5,
    :KC_6,
    :KC_7,
    :KC_8,
    :KC_9,
    :KC_0,
    :KC_ENTER,
    :KC_ESCAPE,
    :KC_BSPACE,
    :KC_TAB,
    :KC_SPACE,
    :KC_MINUS,
    :KC_EQUAL,
    :KC_LBRACKET,
    :KC_RBRACKET,         # 0x30
    :KC_BSLASH,
    :KC_NONUS_HASH,
    :KC_SCOLON,
    :KC_QUOTE,
    :KC_GRAVE,
    :KC_COMMA,
    :KC_DOT,
    :KC_SLASH,
    :KC_CAPSLOCK,
    :KC_F1,
    :KC_F2,
    :KC_F3,
    :KC_F4,
    :KC_F5,
    :KC_F6,
    :KC_F7,               # 0x40
    :KC_F8,
    :KC_F9,
    :KC_F10,
    :KC_F11,
    :KC_F12,
    :KC_PSCREEN,
    :KC_SCROLLLOCK,
    :KC_PAUSE,
    :KC_INSERT,
    :KC_HOME,
    :KC_PGUP,
    :KC_DELETE,
    :KC_END,
    :KC_PGDOWN,
    :KC_RIGHT,
    :KC_LEFT,             # 0x50
    :KC_DOWN,
    :KC_UP,
    :KC_NUMLOCK,
    :KC_KP_SLASH,
    :KC_KP_ASTERISK,
    :KC_KP_MINUS,
    :KC_KP_PLUS,
    :KC_KP_ENTER,
    :KC_KP_1,
    :KC_KP_2,
    :KC_KP_3,
    :KC_KP_4,
    :KC_KP_5,
    :KC_KP_6,
    :KC_KP_7,
    :KC_KP_8,             # 0x60
    :KC_KP_9,
    :KC_KP_0,
    :KC_KP_DOT,
    :KC_NONUS_BSLASH,
    :KC_APPLICATION,
    :KC_POWER,
    :KC_KP_EQUAL,
    :KC_F13,
    :KC_F14,
    :KC_F15,
    :KC_F16,
    :KC_F17,
    :KC_F18,
    :KC_F19,
    :KC_F20,
    :KC_F21,              # 0x70
    :KC_F22,
    :KC_F23,
    :KC_F24,
    :KC_EXECUTE,
    :KC_HELP,
    :KC_MENU,
    :KC_SELECT,
    :KC_STOP,
    :KC_AGAIN,
    :KC_UNDO,
    :KC_CUT,
    :KC_COPY,
    :KC_PASTE,
    :KC_FIND,
    :KC_KB_MUTE,
    :KC_KB_VOLUME_UP,                 # 0x80
    :KC_KB_VOLUME_DOWN,
    :KC_LOCKING_CAPS_LOCK,
    :KC_LOCKING_NUM_LOCK,
    :KC_LOCKING_SCROLL_LOCK,
    :KC_KP_COMMA,
    :KC_KP_EQUAL_AS400,
    :KC_INT1,
    :KC_INT2,
    :KC_INT3,
    :KC_INT4,
    :KC_INT5,
    :KC_INT6,
    :KC_INT7,
    :KC_INT8,
    :KC_INT9,
    :KC_LANG1,            # 0x90
    :KC_LANG2,
    :KC_LANG3,
    :KC_LANG4,
    :KC_LANG5,
    :KC_LANG6,
    :KC_LANG7,
    :KC_LANG8,
    :KC_LANG9,
    :KC_ALT_ERASE,
    :KC_SYSREQ,
    :KC_CANCEL,
    :KC_CLEAR,
    :KC_PRIOR,
    :KC_RETURN,
    :KC_SEPARATOR,
    :KC_OUT,              # 0xA0
    :KC_OPER,
    :KC_CLEAR_AGAIN,
    :KC_CRSEL,
    :KC_EXSEL,
  ]

  # Keycodes with SHIFT modifier
  KEYCODE_SFT = {
    KC_EXLM:           0x1e,
    KC_AT:             0x1f,
    KC_HASH:           0x20,
    KC_DLR:            0x21,
    KC_PERC:           0x22,
    KC_CIRC:           0x23,
    KC_AMPR:           0x24,
    KC_ASTER:          0x25,
    KC_LPRN:           0x26,
    KC_RPRN:           0x27,
    KC_UNDS:           0x2d,
    KC_PLUS:           0x2e,
    KC_LCBR:           0x2f,
    KC_RCBR:           0x30,
    KC_PIPE:           0x31,
  #  KC_TILD:           0x32,
    KC_COLON:          0x33,
    KC_DQUO:           0x34,
    KC_TILD:           0x35,
    KC_LABK:           0x36,
    KC_RABK:           0x37,
    KC_QUES:           0x38,
  }

end

#
# Keyboard class have to be split to avoid unexpected behavior of the compiler
#

class Keyboard

  letter = [
    nil,nil,nil,nil,
    'a', # 0x04
    'b',
    'c',
    'd',
    'e',
    'f',
    'g',
    'h',
    'i',
    'j',
    'k',
    'l',
    'm', # 0x10
    'n',
    'o',
    'p',
    'q',
    'r',
    's',
    't',
    'u',
    'v',
    'w',
    'x',
    'y',
    'z',
    '1',
    '2',
    '3', # 0x20
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    :ENTER,
    :ESCAPE,
    :BSPACE,
    :TAB,
    ' ',
    '-',
    '=',
    '[',
    ']', # 0x30
    "\\",
    nil, # ???
    ';',
    "'",
    '`',
    ',',
    '.',
    '/'
  ]
  letter[74] = :HOME
  letter += [
    :PGUP,
    :DELETE,
    :END,
    :PGDOWN,
    :RIGHT,
    :LEFT,   # 0x50
    :DOWN,
    :UP    # 82
  ]
  LETTER = letter + [
    'A',
    'B',
    'C',
    'D',
    'E',
    'F',
    'G',
    'H',
    'I',
    'J',
    'K',
    'L',
    'M', # 0x10
    'N',
    'O',
    'P',
    'Q',
    'R',
    'S',
    'T',
    'U',
    'V',
    'W',
    'X',
    'Y',
    'Z',
    '!',
    '@',
    '#',
    '$',
    '%',
    '^',
    '&',
    '*',
    '(',
    ')',
    '_',
    '+',
    '{',
    '}',
    '|',
    nil, # KC_TILD
    ':',
    '"',
    '~',
    '<',
    '>',
    '?'
  ]
  KC_ALIASES = {
    KC_ENT: :KC_ENTER,
    KC_ESC: :KC_ESCAPE,
    KC_BSPC: :KC_BSPACE,
    KC_SPC: :KC_SPACE,
    KC_MINS: :KC_MINUS,
    KC_EQL: :KC_EQUAL,
    KC_LBRC: :KC_LBRACKET,
    KC_RBRC: :KC_RBRACKET,
    KC_BSLS: :KC_BSLASH,
    # KC_NUHS: :KC_NONUS_HASH,
    KC_SCLN: :KC_SCOLON,
    KC_QUOT: :KC_QUOTE,
    KC_GRV: :KC_GRAVE,
    KC_ZKHK: :KC_GRAVE,
    KC_COMM: :KC_COMMA,
    KC_SLSH: :KC_SLASH,
    # KC_NUBS: :KC_NONUS_BSLASH,
    # KC_CLCK: :KC_CAPSLOCK,
    KC_CAPS: :KC_CAPSLOCK,
    KC_SLCK: :KC_SCROLLLOCK,
    # KC_BRMD: :KC_SCROLLLOCK,
    KC_NLCK: :KC_NUMLOCK,
    # KC_LCTRL: :KC_LCTL,
    # KC_LSHIFT: :KC_LSFT,
    # KC_LOPT: :KC_LALT,
    # KC_LCMD: :KC_LGUI,
    # KC_LWIN: :KC_LGUI,
    # KC_RCTRL: :KC_RCTL,
    # KC_RSHIFT: :KC_RSFT,
    # KC_ROPT: :KC_RALT,
    # KC_ALGR: :KC_RALT,
    # KC_RCMD: :KC_RGUI,
    # KC_RWIN: :KC_RGUI,
    KC_KANA: :KC_INT2,
    KC_HENK: :KC_INT4,
    KC_MHEN: :KC_INT5,
    KC_HAEN: :KC_LANG1,
    KC_HANJ: :KC_LANG2,
    # KC_PSCR: :KC_PSCREEN,
    # KC_PAUS: :KC_PAUSE,
    # KC_BRK: :KC_PAUSE,
    # KC_BRMU: :KC_PAUSE,
    KC_INS: :KC_INSERT,
    KC_DEL: :KC_DELETE,
    KC_PGDN: :KC_PGDOWN,
    KC_RGHT: :KC_RIGHT,
    # KC_APP: :KC_APPLICATION,
    # KC_EXEC: :KC_EXECUTE,
    # KC_SLCT: :KC_SELECT,
    # KC_AGIN: :KC_AGAIN,
    # KC_PSTE: :KC_PASTE,
    # KC_ERAS: :KC_ALT_ERASE,
    # KC_CLR: :KC_CLEAR,
    # KC_PSLS: :KC_KP_SLASH,
    # KC_PAST: :KC_KP_ASTERISK,
    # KC_PMNS: :KC_KP_MINUS,
    # KC_PPLS: :KC_KP_PLUS,
    # KC_PENT: :KC_KP_ENTER,
    # KC_P1: :KC_KP_1,
    # KC_P2: :KC_KP_2,
    # KC_P3: :KC_KP_3,
    # KC_P4: :KC_KP_4,
    # KC_P5: :KC_KP_5,
    # KC_P6: :KC_KP_6,
    # KC_P7: :KC_KP_7,
    # KC_P8: :KC_KP_8,
    # KC_P9: :KC_KP_9,
    # KC_P0: :KC_KP_0,
    # KC_PDOT: :KC_KP_DOT,
    # KC_PEQL: :KC_KP_EQUAL,
    XXXXXXX: :KC_NO,
  }
  letter = nil
end

# Keyboard class have to be split to avoid unexpected behavior of the compiler

README = "/README.txt"
KEYMAP = "/keymap.rb"
PRK_CONF = "/prk-conf.txt"
DRIVE_NAME = "PRK DRIVE"
MY_VOLUME = FAT.new(:flash, DRIVE_NAME)

class Keyboard

  def self.mount_volume
    return if PRK_NO_MSC
    begin
      VFS.mount(MY_VOLUME, "/")
    rescue => e
      MY_VOLUME.mkfs
      VFS.mount(MY_VOLUME, "/")
    end
    File.open(README, "w") do |f|
      f.puts PRK_DESCRIPTION,
        "\nWelcome to PRK Firmware!\n",
        "Usage:",
        "- Drag and drop your `keymap.rb` into this directory",
        "- Then, your keyboard will be automatically rebooted. That's all!\n",
        "Notice:",
        "- Make sure you always have a backup of your `keymap.rb`",
        "  because upgrading prk_firmware-*.uf2 may remove it from flash\n",
        "https://github.com/picoruby/prk_firmware"
    end
  end

  def self.restart
    VFS.unmount(MY_VOLUME, true)
    USB.hid_task(0, "\000\000\000\000\000\000")
    200.times do
      USB.tud_task
      sleep_ms 1
    end
    VFS.mount(MY_VOLUME, "/")
    # Spec: https://github.com/picoruby/prk_firmware/wiki/VIA-and-Remap#prk-conftxt
    if File.exist?(PRK_CONF)
      File.open(PRK_CONF, "r") do |f|
        line = f.gets&.chomp
        USB.save_prk_conf(line || "::")
      end
    else
      USB.save_prk_conf("::")
    end
    puts "==============================================="
    puts PRK_DESCRIPTION
    puts "PRK_NO_MSC: #{PRK_NO_MSC}"
    puts "prk-conf: #{USB.prk_conf}"
    puts "==============================================="
    if Task[:keyboard].nil?
      File.exist?(KEYMAP) ? Keyboard.reload_keymap : Keyboard.wait_keymap
    elsif File.exist?(KEYMAP) && (0 < File.stat(KEYMAP).mode & FAT::AM_ARC)
      # FIXME: Checking Stat#mode doesn't work well
      Task[:keyboard].suspend
      Keyboard.reload_keymap
    end
    PicoRubyVM.print_alloc_stats
    Keyboard.autoreload_off
  end

  def self.reload_keymap
    $rgb&.turn_off
    script = ""
    File.open(KEYMAP) do |f|
      f.each_line { |line| script << line }
    end
    task = Task[:keyboard] || Task.new(:keyboard)
    task.compile_and_run(script, true)
    File.chmod(0, KEYMAP)
  end

  def self.wait_keymap
    puts "No keymap.rb found"
    script = "while true;sleep 5;puts 'Please make keymap.rb in #{DRIVE_NAME}';end"
    task = Task[:keyboard] || Task.new(:keyboard)
    task.compile_and_run(script, false)
  end

  def initialize
    puts "Init Keyboard"
    # mruby/c VM doesn't work with a CONSTANT to make another CONSTANT
    # steep doesn't allow dynamic assignment of CONSTANT
    @SHIFT_LETTER_THRESHOLD_A    = LETTER.index('A').to_i
    @SHIFT_LETTER_OFFSET_A       = @SHIFT_LETTER_THRESHOLD_A - KEYCODE.index(:KC_A).to_i
    @SHIFT_LETTER_THRESHOLD_UNDS = LETTER.index('_').to_i
    @SHIFT_LETTER_OFFSET_UNDS    = @SHIFT_LETTER_THRESHOLD_UNDS - KEYCODE_SFT[:KC_UNDS]
    @keymaps = Hash.new
    @composite_keys = Array.new
    @mode_keys = Hash.new
    @switches = Array.new
    @injected_switches = Array.new
    @layer_names = Array.new
    @layer = :default
    @split = false
    @split_style = :standard_split
    @anchor = true
    @anchor_left = true # so-called "master left"
    @uart_pin = 1
    @encoders = Array.new
    @partner_encoders = Array.new
    @macro_keycodes = Array.new
    @scan_mode = :matrix
    @skip_positions = Array.new
    @layer_changed_delay = 20
  end

  attr_accessor :split, :uart_pin, :default_layer, :sounder
  attr_reader :layer, :split_style, :cols_size, :rows_size

  def anchor?
    @anchor
  end

  def bootsel!
    puts "Rebooting into BOOTSEL mode!"
    sleep 0.1
    Machine.reset_usb_boot
  end

  def set_debounce(type)
    @debouncer = case type
    when :none
      DebounceNone.new
    when :per_row
      if @scan_mode == :direct
        puts "Warning: Debouncer :per_row won't work where :direct scan mode."
      end
      DebouncePerRow.new
    when :per_key
      DebouncePerKey.new
    end
  end

  def set_debounce_threshold(val)
    @debouncer.threshold = val if @debouncer
  end

  def via=(val)
    if val
      @via = VIA.new
      @via.kbd = self
    end
  end

  def via_layer_count=(count)
    # @type ivar @via: VIA
    @via.layer_count = count
  end

  # TODO: OLED, SDCard
  def append(feature)
    case feature.class.to_s
    when "Mouse"
      # @type var feature: Mouse
      @mouse = feature
    when "RGB"
      # @type var feature: RGB
      $rgb = feature
      $rgb.anchor = @anchor
      if @split
        if @anchor==@anchor_left
          # left
          $rgb.ws2812_circle_set_center(224,32)
        else
          # right
          if @split_style == :right_side_flipped_split
            $rgb.ws2812_circle_set_center(0,32)
          else
            $rgb.ws2812_circle_set_center(224,32)
          end
        end
      else
        $rgb.ws2812_circle_set_center(112,32)
      end
      $rgb.start
    when "RotaryEncoder"
      # @type var feature: RotaryEncoder
      if @split
        feature.create_keycodes(@partner_encoders.size)
        if @anchor_left
          if @anchor == feature.left? # XNOR
            feature.init_pins
            @encoders << feature
          end
        else
          if @anchor != feature.left? #XOR
            feature.init_pins
            @encoders << feature
          end
        end
        if @anchor && (@anchor_left != feature.left?)
          @partner_encoders << feature
        end
      else
        feature.init_pins
        @encoders << feature
      end
    when "VIA"
      # @type var feature: VIA
      feature.kbd = self
      @via = feature
    when "Joystick"
      # @type var feature: Joystick
      @joystick = feature
    when "Sounder"
      # @type var feature: Sounder
      @sounder = feature
    end
  end

  # val should be treated as `:left` if it's anything other than `:right`
  def set_anchor(val)
    @anchor_left = (val != :right)
  end

  def split_style=(style)
    case style
    when :standard_split, :right_side_flipped_split
      @split_style = style
    else
      # NOTE: fall back
      @split_style = :standard_split
    end
  end

  def set_scan_mode(mode)
    case mode
    when :matrix
      @scan_mode = mode
    when :direct
      if @debouncer.is_a?(DebouncePerRow)
        puts "Warning: Scan mode :direct won't work with :per_row debouncer."
      end
      @scan_mode = mode
    end
  end

  def init_uart
    return unless @split
    print "Configured as a split-type"
    @anchor = USB.tud_mounted?
    if @anchor
      puts " Anchor"
      uart_anchor_init(@uart_pin)
    else
      puts " Partner"
      uart_partner_init(@uart_pin)
    end
  end

  def mutual_uart_at_my_own_risk=(_v)
    puts
    puts "[Deprecated in 0.9.20]"
    puts "Keyboard#mutual_uart_at_my_own_risk= no longer takes any effect."
    puts "Software-mutual-UART has became the default and only option."
    puts
  end

  def init_matrix_pins(matrix)
    puts "Init GPIO"
    init_uart
    @cols_size = 0
    @rows_size = matrix.size
    @matrix = Hash.new
    matrix.each do |cols|
      @cols_size = [cols.size, @cols_size].max.to_i
      cols.each do |cell|
        if cell.is_a?(Array)
          @matrix[cell[0]] = Hash.new unless @matrix[cell[0]]
        end
      end
    end
    matrix.each_with_index do |rows, row_index|
      rows.each_with_index do |cell, col_index|
        if cell.is_a?(Array)
          @matrix[cell[0]][cell[1]] = [row_index, col_index]
          gpio_init(cell[0])
          gpio_set_dir(cell[0], GPIO_IN_PULLUP)
          gpio_init(cell[1])
          gpio_set_dir(cell[1], GPIO_IN_PULLUP)
        else # should be nil
          @skip_positions << [row_index, col_index]
        end
      end
    end
    @offset_a = (@cols_size / 2.0).ceil_to_i
    @offset_b = @cols_size * 2 - @offset_a - 1
  end

  def init_pins(rows, cols)
    @rows_size = rows.size
    matrix = Array.new
    rows.each do |row|
      line = Array.new
      cols.each do |col|
        line << [row, col]
      end
      matrix << line
    end
    init_matrix_pins matrix
  end

  def init_direct_pins(pins)
    set_scan_mode :direct
    init_uart
    pins.each do |pin|
      gpio_init(pin)
      gpio_set_dir(pin, GPIO_IN_PULLUP)
    end
    @cols_size = pins.count
    @direct_pins = pins
    @offset_a = (@cols_size / 2.0).ceil_to_i
    @offset_b = @cols_size * 2 - @offset_a - 1
  end

  def skip_position?(row, col)
    col2 = if col < @cols_size
      col
    else
      if @split_style == :right_side_flipped_split
        col - @cols_size
      else # :standard_split
        (col - @cols_size + 1) * -1 + @cols_size
      end
    end
    @skip_positions.include?([row, col2])
  end

  # Input
  #   name:    default, map: [ [ :KC_A, :KC_B, :KC_LCTL,   :MACRO_1 ],... ]
  # ↓
  # Result
  #   layer: { default:      [ [ -0x04, -0x05, 0b00000001, :MACRO_1 ],... ] }
  def add_layer(name, map)
    # The name that is added at first is going to be the default name
    @default_layer ||= name
    new_map = Array.new
    new_map[0] = Array.new
    row_index = 0
    col_index = 0
    map.each do |key|
      if entire_cols_size <= col_index
        row_index += 1
        col_index = 0
        new_map[row_index] = Array.new
      end
      while skip_position?(row_index, col_index)
        new_map[row_index][calculate_col_position(col_index)] = 0
        col_index += 1
        if entire_cols_size <= col_index
          row_index += 1
          col_index = 0
          new_map[row_index] = Array.new
        end
      end
      col_position = calculate_col_position(col_index)
      case key.class
      when Symbol
        # @type var map: Array[Symbol]
        # @type var key: Symbol
        new_map[row_index][col_position] = find_keycode_index(key)
      when Array
        # @type var map: Array[Array[Symbol]]
        # @type var key: Array[Symbol]
        composite_key = key.join("_").to_sym
        new_map[row_index][col_position] = composite_key
        @composite_keys << {
          layer: name,
          keycodes: convert_composite_keys(key),
          switch: [row_index, col_position]
        }
      end
      col_index += 1
    end
    @keymaps[name] = new_map
    @layer_names << name unless @layer_names.include?(name)
  end

  def get_layer(name, num)
    if name
      return @keymaps[name] if @keymaps[name]
    end

    if @layer_names.size>num
      return @keymaps[@layer_names[num]]
    end

    return nil
  end

  def delete_mode_keys(layer_name)
    @composite_keys.delete_if { |item| item[:layer]==layer_name }
    @mode_keys.each do |switch, config|
      config[:layers].delete(layer_name)
    end
  end

  def entire_cols_size
    @entire_cols_size ||= @split ? @cols_size * 2 : @cols_size
  end

  def calculate_col_position(col_index)
    return col_index unless @split
    case @split_style
    when :right_side_flipped_split
      if col_index < @cols_size
        col_index
      else
        entire_cols_size - (col_index - @cols_size) - 1
      end
    else
      # Should not happen but be guarded to pass steep check.
      col_index
    end
  end

  def resolve_key_alias(sym)
    KC_ALIASES[sym] || sym
  end

  def find_keycode_index(key)
    key = resolve_key_alias(key)
    keycode = KEYCODE.index(key)
    if keycode
      keycode * -1
    elsif keycode = KEYCODE_SFT[key]
      (keycode + 0x100) * -1
    elsif keycode = MOD_KEYCODE[key]
      keycode
    elsif required?("mouse") && keycode = Mouse::KEYCODE[key]
      keycode
    elsif required?("rgb") && keycode = RGB::KEYCODE[key]
      keycode
    elsif required?("consumer_key") && keycode = ConsumerKey.keycode(key)
      # You need to `require "consumer_key"`
      keycode + ConsumerKey::MAP_OFFSET
    elsif key.to_s.start_with?("JS_BUTTON")
      # JS_BUTTON0 - JS_BUTTON31
      # You need to `require "joystick"`
      key.to_s[9, 10].to_i + 0x200
    elsif key.to_s.start_with?("JS_HAT_")
      case key
      when :JS_HAT_RIGHT
        0b0001 + 0x100
      when :JS_HAT_UP
        0b0010 + 0x100
      when :JS_HAT_DOWN
        0b0100 + 0x100
      when :JS_HAT_LEFT
        0b1000 + 0x100
      else
        key # Symbol (possibly user defined)
      end
    else
      key # Symbol (should be user defined)
    end
  end

  def convert_composite_keys(keys)
    keys.map do |sym|
      sym = resolve_key_alias(sym)
      keycode_index = KEYCODE.index(sym)
      if keycode_index
        keycode_index * -1
      elsif KEYCODE_SFT[sym]
        (KEYCODE_SFT[sym] + 0x100) * -1
      else # Should be a modifier
        MOD_KEYCODE[sym]
      end
    end
  end

  def define_composite_key(key_name, keys)
    @keymaps.each do |layer, map|
      map.each_with_index do |row, row_index|
        row.each_with_index do |key_symbol, col_index|
          if key_name == key_symbol
            @composite_keys << {
              layer: layer,
              keycodes: convert_composite_keys(keys),
              switch: [row_index, col_index]
            }
          end
        end
      end
    end
  end

  # param[0] :on_release
  # param[1] :on_hold
  # param[2] :release_threshold
  # param[3] :repush_threshold
  def define_mode_key(key_name, param, from_via = false)
    if @via && !from_via
      @via.add_mode_key(key_name, param)
      return
    end
    on_release = param[0]
    on_hold = param[1]
    release_threshold = param[2]
    repush_threshold = param[3]
    @keymaps.each do |layer, map|
      map.each_with_index do |row, row_index|
        row.each_with_index do |key_symbol, col_index|
          next if key_symbol != key_name
          on_release_action = case on_release.class
          when Symbol
            # @type var on_release: Symbol
            key = resolve_key_alias(on_release)
            keycode_index = KEYCODE.index(key)
            if keycode_index
              keycode_index * -1
            elsif KEYCODE_SFT[key]
              (KEYCODE_SFT[key] + 0x100) * -1
            end
          when Array
            # @type var on_release: Array[Symbol]
            convert_composite_keys(on_release)
          when Proc
            # @type var on_release: Proc
            on_release
          end
          on_hold_action = if on_hold.is_a?(Symbol)
            MOD_KEYCODE[on_hold] ? MOD_KEYCODE[on_hold] : on_hold
          else
            # @type var on_hold: Proc
            on_hold
          end
          switch = [row_index, col_index]
          # @type var switch: [Integer, Integer]
          @mode_keys[switch] ||= {
            prev_state:  :released,
            pushed_at:   0,
            released_at: 0,
            layers:      Hash.new
          }
          @mode_keys[switch][:layers][layer] = {
            on_release:        on_release_action,
            on_hold:           on_hold_action,
            release_threshold: (release_threshold || 0),
            repush_threshold:  (repush_threshold || 0)
          }
        end
      end
    end
  end

  # MOD_KEYCODE = {
  #        KC_LCTL: 0b00000001,
  #        KC_LSFT: 0b00000010,
  #        KC_LALT: 0b00000100,
  #        KC_LGUI: 0b00001000,
  #        KC_RCTL: 0b00010000,
  #        KC_RSFT: 0b00100000,
  #        KC_RALT: 0b01000000,
  #        KC_RGUI: 0b10000000
  # }
  def invert_sft
    if (@modifier & 0b00100010) > 0
       @modifier &= 0b11011101
    else
       @modifier |= 0b00000010
    end
  end

  def before_report(&block)
    @before_filters ||= Array.new
    # @type ivar @before_filters: Array[^() -> void]
    @before_filters << block
  end

  def on_start(&block)
    @on_start_procs ||= Array.new
    # @type ivar @on_start_procs: Array[^() -> void]
    @on_start_procs << block
  end

  SIGNALS_MAX_INDEX = 7

  def signal_partner(key, &block)
    return unless @split
    @partner_signals ||= Array.new
    @anchor_signals ||= Array.new
    # @type ivar @partner_signals: Array[^() -> void]
    # @type ivar @anchor_signals: Array[Symbol]
    if SIGNALS_MAX_INDEX == @partner_signals.count
      puts "No more signals can register"
      return
    end
    @partner_signals << block
    @anchor_signals << key
  end

  def keys_include?(key)
    keycode = KEYCODE.index(key)
    !keycode.nil? && @keycodes.include?(keycode.chr)
  end

  def key_pressed?
    @key_pressed
  end

  def action_on_release(mode_key)
    case mode_key.class
    when Integer
      # @type var mode_key: Integer
      if mode_key < -255
        @keycodes << ((mode_key + 0x100) * -1).chr
        @modifier |= 0b00000010
      else
        @keycodes << (mode_key * -1).chr
      end
    when Array
      0 # `steep check` will fail if you remove this line 🤔
      # @type var mode_key: Array[Integer]
      mode_key.each do |key|
        if key < -255
          @keycodes << ((key + 0x100) * -1).chr
          @modifier |= 0b00000010
        elsif key < 0
          @keycodes << (key * -1).chr
        else # Should be a modifier
          @modifier |= key
        end
      end
    when Proc
      # @type var mode_key: Proc
      mode_key.call
    end
  end

  # for Encoders
  def send_key(*args)
    symbols = case args[0].class
    when Symbol
      args
    when Array
      args[0]
    else
      puts "[WARN] Argument for send_key is wrong"
      []
    end
    modifier = 0
    consumer_reported = false
    keycodes = "\000\000\000\000\000\000"
    if required?("rgb") && RGB::KEYCODE[symbols[0]]
      $rgb&.invoke_anchor(symbols[0])
      return
    elsif required?("consumer_key") && keycode = ConsumerKey.keycode(symbols[0])
      USB.merge_consumer_report(keycode)
      consumer_reported = true
    elsif keycode = KEYCODE_SFT[symbols[0]]
      modifier = 0b00100000
      keycodes = "#{keycode.chr}\000\000\000\000\000"
    else
      6.times do |i|
        break i unless symbols[i]
        if code = KEYCODE.index(symbols[i])
         keycodes[i] = code.chr
        elsif code = MOD_KEYCODE[symbols[i]]
          modifier |= code
        end
      end
    end
    USB.hid_task(modifier, keycodes)
    sleep_ms(consumer_reported ? 4 : 1)
    USB.hid_task(0, "\000\000\000\000\000\000")
  end

  def output_report_changed(&block)
    @output_report_cb = block
    USB.start_observing_output_report
  end

  def inject_switch(col, row)
    @injected_switches << [row, col]
  end

  # **************************************************************
  #  For those who are willing to contribute to PRK Firmware:
  #
  #   The author has intentionally made this method big and
  #   redundant for resilience against a change of spec.
  #   Please refrain from "refactoring" for a while.
  # **************************************************************
  def start!
    # If keymap.rb didn't set any debouncer,
    # default debounce algorithm will be applied
    self.set_debounce(@scan_mode == :direct ? :per_key : :per_row) unless @debouncer

    puts "Keyboard started"

    @on_start_procs&.each{ |my_proc| my_proc.call }

    # To avoid unintentional report on startup
    # which happens only on Sparkfun Pro Micro RP2040
    if @split && @anchor
      sleep_ms 500
      while true
        data = uart_anchor(0)
        break if data == 0xFFFFFF
      end
    end

    @via&.start!

    @keycodes = Array.new
    prev_layer = @default_layer
    modifier_switch_positions = Array.new
    message_to_partner, earlier_report_size, prev_output_report = 0, 0, 0

    joystick_hat, joystick_buttons = 0, 0

    mouse_buttons,
    mouse_cursor_x,
    mouse_cursor_y,
    mouse_wheel_x,
    mouse_wheel_y = 0, 0, 0, 0, 0

    while true
      cycle_time = 20
      now = Machine.board_millis
      @keycodes.clear

      @switches = @injected_switches.dup
      @injected_switches.clear
      @modifier = 0

      @scan_mode == :matrix ? scan_matrix! : scan_direct!
      @key_pressed = !@switches.empty? # Independent even on split type

      # TODO: more features
      $rgb.fifo_push(true) if $rgb && !@switches.empty?

      if @anchor
        # Receive max 3 switches from partner
        if @split
          sleep_ms 3
          if message_to_partner > 0
            data24 = uart_anchor(message_to_partner)
            message_to_partner = 0
          elsif $rgb && $rgb.ping?
            data24 = uart_anchor(0b11100000) # adjusts RGB time
          else
            data24 = uart_anchor(0)
          end
          [data24 & 0xFF, (data24 >> 8) & 0xFF, data24 >> 16].each do |data|
            if data == 0xFF
              # do nothing
            elsif data > 246
              @partner_encoders.each { |encoder| encoder.call_proc_if(data) }
            else
              switch = [data >> 5, data & 0b00011111]
              # To avoid chattering
              # @type var switch: [Integer, Integer]
              @switches << switch unless @switches.include?(switch)
            end
          end
        end

        desired_layer = @layer
        @composite_keys.each do |composite_key|
          next if composite_key[:layer] != @layer
          if @switches.include?(composite_key[:switch])
            action_on_release(composite_key[:keycodes])
          end
        end

        @mode_keys.each do |switch, mode_key|
          layer_action = mode_key[:layers][@layer || @default_layer]
          next unless layer_action
          if @switches.include?(switch)
            on_hold = layer_action[:on_hold]
            case mode_key[:prev_state]
            when :released, :pushed, :pushed_interrupted
              if mode_key[:prev_state] == :released
                if earlier_report_size == 0
                  mode_key[:pushed_at] = now
                  mode_key[:prev_state] = :pushed
                else
                  on_hold = nil # To skip hold
                end
              elsif earlier_report_size > 0
                # To prevent from invoking action_on_release
                mode_key[:prev_state] = :pushed_interrupted
              end
              case on_hold.class
              when Symbol
                # @type var on_hold: Symbol
                desired_layer = on_hold
              when Integer
                # @type var on_hold: Integer
                @modifier |= on_hold
              when Proc
                # @type var on_hold: Proc
                on_hold.call
              end
            when :pushed_then_released
              if now - mode_key[:released_at] <= layer_action[:repush_threshold]
                mode_key[:prev_state] = :pushed_then_released_then_pushed
              end
            when :pushed_then_released_then_pushed
              action_on_release(layer_action[:on_release])
            end
          else
            case mode_key[:prev_state]
            when :pushed
              if (earlier_report_size == 0) &&
                  (now - mode_key[:pushed_at] <= layer_action[:release_threshold])
                action_on_release(layer_action[:on_release])
                mode_key[:prev_state] = (layer_action[:repush_threshold]==0) ? :released : :pushed_then_released
              else
                mode_key[:prev_state] = :released
              end
              mode_key[:released_at] = now
              @layer = prev_layer
              prev_layer = @default_layer
            when :pushed_interrupted, :pushed_then_released_then_pushed
              mode_key[:prev_state] = :released
            when :pushed_then_released
              if now - mode_key[:released_at] > layer_action[:release_threshold]
                mode_key[:prev_state] = :released
              end
            end
          end
        end

        if @layer != desired_layer
          prev_layer = @layer || @default_layer if prev_layer != @default_layer
          @layer = desired_layer
        end

        keymap = @keymaps[@locked_layer || @layer || @default_layer]
        modifier_switch_positions.clear
        @switches.each_with_index do |switch, i|
          keycode = keymap[switch[0]][switch[1]]
          if signal = @anchor_signals&.index(keycode)
            message_to_partner = signal + 1
          end
          next unless keycode.is_a?(Integer)
          # Reserved keycode range
          #        ..-0x100 : Key with shift
          #  -0x0FF..-0x001 : Normal key
          #       0.. 0x100 : Modifier key
          #   0x101.. 0x1FF : Joystick D-pad hat
          #   0x200.. 0x2FF : Joystick button
          #   0x300.. 0x3FF : Mouse
          #   0x400.. 0x6FF : Consumer (media) key
          #   0x700.. 0x7FF : RGB
          if keycode < -0xFF
            @keycodes << ((keycode + 0x100) * -1).chr
            @modifier |= 0b00100000
          elsif keycode < 0
            @keycodes << (keycode * -1).chr
          elsif keycode < 0x100
            @modifier |= keycode
            modifier_switch_positions.unshift i
          elsif keycode < 0x200
            joystick_hat |= (keycode - 0x100)
          elsif keycode < 0x300
            joystick_buttons |= (1 << (keycode - 0x200))
          elsif keycode < 0x400
            if keycode < 0x306 # Mouse button
              # @type var mouse_buttons: Integer
              mouse_buttons |= (keycode - 0x300)
            elsif keycode == 0x311 # Mouse UP
              mouse_cursor_y = @mouse.cursor_speed
            elsif keycode == 0x312 # Mouse DOWN
              mouse_cursor_y = -@mouse.cursor_speed
            elsif keycode == 0x313 # Mouse LEFT
              mouse_cursor_x = @mouse.cursor_speed
            elsif keycode == 0x314 # Mouse RIGHT
              mouse_cursor_x = -@mouse.cursor_speed
            elsif keycode == 0x315 # Mouse WHEEL UP
              mouse_wheel_y = @mouse.wheel_speed
            elsif keycode == 0x316 # Mouse WHEEL DOWN
              mouse_wheel_y = -@mouse.wheel_speed
            elsif keycode == 0x317 # Mouse WHEEL LEFT
              mouse_wheel_x = @mouse.wheel_speed
            elsif keycode == 0x318 # Mouse WHEEL RIGHT
              mouse_wheel_x = -@mouse.wheel_speed
            end
          elsif keycode < 0x700
            USB.merge_consumer_report ConsumerKey.keycode_from_mapcode(keycode)
          elsif keycode < 0x800
            message_to_partner = $rgb&.invoke_anchor(RGB::KEYCODE.key(keycode)) || 0
          else
            puts "[ERROR] Wrong keycode: 0x#{keycode.to_s(16)}"
          end
        end
        # To fix https://github.com/picoruby/prk_firmware/issues/49
        modifier_switch_positions.each do |i|
          @switches.delete_at(i)
        end

        # Macro
        macro_keycode = @macro_keycodes.shift
        if macro_keycode
          if macro_keycode < 0
            @modifier |= 0b00100000
            @keycodes << (macro_keycode * -1).chr
          else
            @keycodes << macro_keycode.chr
          end
          cycle_time = 40 # To avoid accidental skip
        end

        earlier_report_size = @keycodes.size
        (6 - earlier_report_size).times do
          @keycodes << "\000"
        end

        @before_filters&.each do |block|
          block.call
        end

        @encoders.each do |encoder|
          encoder.consume_rotation_anchor
        end

        if @joystick
          USB.merge_joystick_report(joystick_buttons, joystick_hat)
          joystick_buttons, joystick_hat = 0, 0
        end

        if @mouse
          USB.merge_mouse_report(
            mouse_buttons,
            mouse_cursor_x, mouse_cursor_y,
            mouse_wheel_x, mouse_wheel_y
          )
          mouse_buttons,
          mouse_cursor_x, mouse_cursor_y,
          mouse_wheel_x, mouse_wheel_y = 0, 0, 0, 0, 0
          @mouse.task_proc&.call(@mouse, self)
        end

        USB.hid_task(@modifier, @keycodes.join)

        if @locked_layer
          # @type ivar @locked_layer: Symbol
          @layer = @locked_layer
        elsif @switches.empty?
          @layer = @default_layer
        end
      else
        # Partner
        @before_filters&.each do |block|
          block.call
        end
        @encoders.each do |encoder|
          data = encoder.consume_rotation_partner
          uart_partner_push8(data) if data && data > 0
        end
        @switches.each do |switch|
          # 0b11111111
          #   ^^^      row number (0 to 7)
          #      ^^^^^ col number (0 to 31)
          uart_partner_push8((switch[0] << 5) + switch[1])
        end
        message_from_anchor = uart_partner
        if message_from_anchor == 0
          # Do nothing
        elsif message_from_anchor <= SIGNALS_MAX_INDEX
          @partner_signals&.at(message_from_anchor - 1)&.call
        elsif message_from_anchor <= 0b00011111
          # Reserved
        else
          $rgb&.invoke_partner message_from_anchor
        end
      end

      @via&.task

      # CapsLock, NumLock, etc.
      if prev_output_report != USB.output_report && @output_report_cb
        prev_output_report = USB.output_report
        @output_report_cb.call(prev_output_report)
      end

      time = cycle_time - (Machine.board_millis - now)
      sleep_ms(time) if time > 0
    end

  end

  def scan_matrix!
    @debouncer.set_time
    @matrix.each do |out_pin, in_pins|
      gpio_set_dir(out_pin, GPIO_OUT_LO)
      in_pins.each do |in_pin, switch|
        row = switch[0]
        unless @debouncer.resolve(in_pin, out_pin)
          col = if @anchor_left
            if @anchor
              # left
              switch[1]
            else
              # right
              (switch[1] - @offset_a) * -1 + @offset_b
            end
          else # right side is the anchor
            unless @anchor
              # left
              switch[1]
            else
              # right
              (switch[1] - @offset_a) * -1 + @offset_b
            end
          end
          @switches << [row, col]
        end
      end
      gpio_set_dir(out_pin, GPIO_IN_PULLUP)
    end
  end

  def scan_direct!
    @debouncer.set_time
    @direct_pins.each_with_index do |col_pin, col|
      unless @debouncer.resolve(col_pin, 0)
        col = if @anchor_left
          if @anchor
            # left
            col
          else
            # right
            (col - @offset_a) * -1 + @offset_b
          end
        else # right side is the anchor
          unless @anchor
            # left
            col
          else
            # right
            (col - @offset_a) * -1 + @offset_b
          end
        end
        @switches << [0, col]
      end
    end
  end

  #
  # Actions can be used in keymap.rb
  #

  # Raises layer and keeps it
  def raise_layer
    current_index = @layer_names.index(@locked_layer || @layer)
    return if current_index.nil?
    if current_index < @layer_names.size - 1
      # @type var current_index: Integer
      @locked_layer = @layer_names[current_index + 1]
    else
      @locked_layer = @layer_names.first
    end
  end

  # Lowers layer and keeps it
  def lower_layer
    current_index = @layer_names.index(@locked_layer || @layer)
    return if current_index.nil?
    if current_index == 0
      @locked_layer = @layer_names.last
    else
      # @type var current_index: Integer
      @locked_layer = @layer_names[current_index - 1]
    end
  end

  # Switch to specified layer
  def lock_layer(layer)
    @locked_layer = layer
  end

  def unlock_layer
    @locked_layer = nil
  end

  def macro(text, opts = [:ENTER])
    print text.to_s
    prev_c = ""
    text.to_s.each_char do |c|
      index = LETTER.index(c)
      next unless index
      @macro_keycodes << 0
      if index >= @SHIFT_LETTER_THRESHOLD_UNDS
        @macro_keycodes << (index - @SHIFT_LETTER_OFFSET_UNDS) * -1
      elsif index >= @SHIFT_LETTER_THRESHOLD_A
        @macro_keycodes << (index - @SHIFT_LETTER_OFFSET_A) * -1
      else
        @macro_keycodes << index
      end
    end
    opts.each do |opt|
      @macro_keycodes << 0
      @macro_keycodes << (LETTER.index(opt) || 0)
      puts if opt == :ENTER
    end
  end

end

