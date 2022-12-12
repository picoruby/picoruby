class VIA
  VIA_IDs = %i[
    ID_NONE
    ID_GET_PROTOCOL_VERISON
    ID_GET_KEYBOARD_VALUE
    ID_SET_KEYBOARD_VALUE
    ID_VIA_GET_KEYCODE
    ID_VIA_SET_KEYCODE
    ID_VIA_RESET
    ID_LIGHTING_SET_VALUE
    ID_LIGHTING_GET_VALUE
    ID_LIGHTING_SAVE
    ID_EEPROM_RESET
    ID_BOOTLOADER_JUMP
    ID_VIA_MACRO_GET_COUNT
    ID_VIA_MACRO_GET_BUFFER_SIZE
    ID_VIA_MACRO_GET_BUFFER
    ID_VIA_MACRO_SET_BUFFER
    ID_VIA_MACRO_RESET
    ID_VIA_GET_LAYER_COUNT
    ID_VIA_GET_BUFFER
    ID_VIA_SET_BUFFER
  ]
  ID_UNHANDLED = 0xFF
  VIA_PROTOCOL_VERSION = 0x0009
  KEYBOARD_VALUES = %i[
    ID_NONE
    ID_UPTIME
    ID_LAYOUT_OPTIONS
    ID_SWITCH_MATRIX_STATE
  ]

  VIA_FILENAME = "VIA_MAP TXT"

  def initialize
    puts "Init VIA"
    @layer_count = 5
    @mode_keys = Hash.new
    @keymap_saved = true
    @keymaps = Array.new
    @composite_keys = Array.new
  end

  attr_accessor :layer_count, :kbd, :sandbox

  def cols_size
    (@kbd.cols_size || 1) * (@kbd.split ? 2 : 1)
  end

  def rows_size
    @kbd.rows_size || 1
  end

  def expand_composite_key(name)
    keynames = []
    ary = name.to_s.split("_")
    ary.each do |n|
      keynames << ("KC_"+n).intern
    end
    
    return keynames
  end

  def load_mode_keys
    @mode_keys.each do |key_name, param|
      @kbd.define_mode_key(key_name, param, true)
    end

    @composite_keys.each do |key_name|
      @kbd.define_composite_key(key_name, expand_composite_key(key_name))
    end
  end

  def add_mode_key(key_name, param)
    @mode_keys[key_name] = param
  end

  def get_modifier_name(bits)
    names = "LCTL LSFT LALT LGUI RCTL RSFT RALT RGUI".split
    ary = []
    8.times do |i|
      unless ( bits & (1<<i) )==0
        ary << names[i]
      end
    end
    return ary.join("_")
  end

  def get_via_composite_keycode(keyname)
    names = "LCTL LSFT LALT LGUI RCTL RSFT RALT RGUI".split
    modifier = 0
    via_keycode = 0
    keyname.to_s.split("_").each do |n|
      i = names.index n
      if i
        modifier |= 1<<i
      else
        key_str = "KC_"+n
        prk_keycode = @kbd.find_keycode_index( key_str.intern )
        
        if prk_keycode.class == Integer
          # @type var prk_keycode: Integer
          via_keycode = prk_keycode_into_via_keycode(prk_keycode)
        else
          via_keycode = 0
        end
      end
    end
    
    if 0x0F & modifier == 0
      via_keycode |= (modifier<<4) & 0x0F00
      via_keycode |= 0x1000
    else
      via_keycode |= modifier<<8
    end

    return via_keycode
  end

  # PRK_KeySymbol | PRK_Keycode -> VIA_Keycode
  def prk_keycode_into_via_keycode(key)
    case key.class
    when Integer
      # @type var key: Integer
      if key>0
        if key < 0x100
          8.times do |i|
            if (key>>i) == 1
              return 0xE0 + i
            end
          end
          return 0
        elsif key < 0x101
          return 0
        elsif required?("consumer_key") && key < 0x600
          ConsumerKey.viacode_from_mapcode(key) || 0
        elsif required?("rgb") && key <= (RGB::KEYCODE.values.max || 0x6FF)
          if key < 0x606
            i = [2,3,3,4,4]
            return 0x5CC0 + i[key-0x601]
          else
            return 0x5CC5 + key - 0x606
          end
        else
          return 0
        end
      elsif key<-255
        key = key * (-1) - 0x100
        return (0x12<<8) | key
      else
        return -key
      end
    when Symbol
      # @type var key: Symbol
      if key==:KC_NO
        return 0
      else
        key_str = key.to_s
        if key_str.start_with?("VIA_FUNC")
          return 0x00C0 + key_str.split("C")[1].to_i
        elsif ! key_str.start_with?("KC_")
          return get_via_composite_keycode(key)
        else
          return 0
        end
      end
    else
      return 0
    end
  end

  def check_for_keycode_shift(keycode) 
    if (keycode>>8) & 0x0F == 0x02
      code = keycode & 0x00FF
      return nil if ( code < 0x1e ) || ( 0x38 < code )

      Keyboard::KEYCODE_SFT.each do |keyname, value|
        return keyname if value == code
      end
    end

    return nil
  end

  def via_keycode_into_keysymbol(keycode)
    if (keycode>>8)==0
      if 0x00C0 <= keycode && keycode <= 0x00DF
        c = keycode - 0x00C0
        cs = c.to_s
        s = "VIA_FUNC" + cs
        return s.intern
      elsif keycode<0xE0
        if sym = Keyboard::KEYCODE[keycode & 0x00FF]
          return sym
        elsif required?("consumer_key") && sym = ConsumerKey::VIACODE[keycode & 0x00FF]
          return sym
        else
          :KC_NO
        end
      else
        i = keycode - 0xE0
        return :KC_NO if i>8
        bits = 1<<i
        return ( "KC_"+get_modifier_name(bits) ).intern
      end
    else
      case (keycode>>12) & 0x0F
      when 0x0
        keysymbol = check_for_keycode_shift(keycode)
        return keysymbol if keysymbol

        modifiers = (keycode>>8) & 0x0F
        key = keycode & 0x00FF
        keysymbol = (
          get_modifier_name(modifiers) + "_" + 
          via_keycode_into_keysymbol(key).to_s.split("_")[1] ).intern
        return keysymbol
      when 0x1
        keysymbol = check_for_keycode_shift(keycode)
        return keysymbol if keysymbol

        modifiers = ( (keycode>>8) & 0x0F )<<4
        key = keycode & 0x00FF
        keysymbol = (
          get_modifier_name(modifiers) + "_" + 
          via_keycode_into_keysymbol(key).to_s.split("_")[1] ).intern
        return keysymbol
      when 0x5
        return :KC_NO unless required?("rgb")
        case keycode
        when 0x5CC2
          return :RGB_TOG
        when 0x5CC3
          return :RGB_MOD
        when 0x5CC4
          return :RGB_RMOD
        when (0x5CC5..0x5CCC)
          idx = keycode & 0x0F
          return RGB::KEYCODE.keys[idx]
        end
      end
    end

    return :KC_NO
  end

  def init_keymap
    @layer_count.times do |layer|
      @keymaps[layer] = []
      rows_size.times do |row|
        @keymaps[layer][row] = Array.new(cols_size)
      end
    end
  end

  def sync_keymap(init=false)
    @layer_count.times do |i|
      layer_name = init ? nil : via_get_layer_name(i)
      layer = @kbd.get_layer(layer_name, i)
      if layer
        via_map = []
        layer.each_with_index do |rows, row|
          via_map[row] = []
          rows.each_with_index do |cell, col|
            keycode = cell ? prk_keycode_into_via_keycode(cell) : 0
            if @kbd.split && @kbd.split_style == :right_side_flipped_split && ( col > cols_size / 2 - 1 )
              via_map[row][cols_size - 1 - col + cols_size / 2] = keycode
            else
              via_map[row][col] = keycode
            end
          end
        end
        @keymaps[i] = via_map
      end
    end
  end

  def save_keymap
    save_on_keyboard
    save_on_flash
  end

  def save_on_keyboard
    @layer_count.times do |layer|
      layer_name = via_get_layer_name(layer)
      @kbd.delete_mode_keys(layer_name)
      
      map = Array.new(rows_size * cols_size)
      rows_size.times do |row|
        cols_size.times do |col|
          keysymbol = via_keycode_into_keysymbol(@keymaps[layer][row][col] || 0)
          map[row * cols_size + col] = keysymbol
          
          keyname = keysymbol.to_s

          next if keyname.start_with?("KC")
          next if keyname.start_with?("VIA_FUNC")
          # composite key
          next if @composite_keys.include?(keysymbol)
          @composite_keys << keysymbol
        end
      end
      
      @kbd.add_layer layer_name, map
    end
    load_mode_keys
  end

  def save_on_flash
    data = ""

    @layer_count.times do |i|
      rows_size.times do |j|
        cols_size.times do |k|
          key = via_keycode_into_keysymbol(@keymaps[i][j][k] || 0)
          data << key.to_s + " "
        end

        data << " \n"
      end

      data << ",\n"
    end

    data << ""
    
    write_file_internal(VIA_FILENAME, data)
  end
  
  def start!
    init_keymap

    fileinfo = find_file(VIA_FILENAME)
    
    if fileinfo
      puts "via_map.txt found"
      script = read_file(fileinfo[0], fileinfo[1])
      
      ret = []
      layers = script.split(",")
      layers.size.times do |l|
          ret[l] = []
          rows = layers[l].split("\n")
          rows.each do |row|
              row.split(" ").each do |kc|
                  ret[l] << kc.intern
              end
          end
      end

      puts "loading VIA map"
      # @type var ret: Array[Array[Symbol]]
      ret.each_with_index do |map,i|
        layer_name = via_get_layer_name i
        @kbd.add_layer layer_name, map
        map.each do |kc|
          next if kc.to_s.start_with?("KC")
          next if kc.to_s.start_with?("VIA_FUNC")
          # composite keys
          @composite_keys << kc unless @composite_keys.include?(kc)
        end
      end
      sync_keymap(false)
      load_mode_keys
    else
      sync_keymap(true)
      save_on_keyboard
    end
  end

  def raw_hid_receive_kb(data)
    ary = Array.new(data.size)
    ary[0] = ID_UNHANDLED
    return ary
  end

  def raw_hid_receive(data)
    command_id   = data[0]
    command_name = VIA_IDs[command_id]

    case command_name
    when :ID_GET_PROTOCOL_VERSION
      data[1] = VIA_PROTOCOL_VERSION >> 8
      data[2] = VIA_PROTOCOL_VERSION & 0xFF
    when :ID_GET_KEYBOARD_VALUE
      case KEYBOARD_VALUES[data[1]]
      when :ID_UPTIME
        value  = board_millis >> 10 # seconds
        data[2] = (value >> 24) & 0xFF
        data[3] = (value >> 16) & 0xFF
        data[4] = (value >> 8) & 0xFF
        data[5] = value & 0xFF
      when :ID_LAYOUT_OPTIONS
        value  = 0x12345678 #via_get_layout_options
        data[2] = (value >> 24) & 0xFF
        data[3] = (value >> 16) & 0xFF
        data[4] = (value >> 8) & 0xFF
        data[5] = value & 0xFF
      when :ID_SWITCH_MATRIX_STATE
        # not implemented
      else
        data = raw_hid_receive_kb(data)
      end
    when :ID_SET_KEYBOARD_VALUE
      case KEYBOARD_VALUES[data[1]]
      when :ID_LAYOUT_OPTIONS
        value = (data[2] << 24) | (data[3] << 16)  | (data[4] << 8) | data[5]
        #via_set_layout_options(value);
      else
        data = raw_hid_receive_kb(data)
      end
    when :ID_VIA_GET_KEYCODE
      keycode = dynamic_keymap_get_keycode(data[1], data[2], data[3])
      data[4]  = keycode >> 8
      data[5]  = keycode & 0xFF
    when :ID_VIA_SET_KEYCODE
      dynamic_keymap_set_keycode(data[1], data[2], data[3], (data[4] << 8) | data[5])
    when :ID_VIA_MACRO_GET_COUNT
      data[1] = 0
    when :ID_VIA_MACRO_GET_BUFFER_SIZE
      size   = 0x0 #dynamic_keymap_macro_get_buffer_size()
      data[1] = size >> 8
      data[2] = size & 0xFF
    when :ID_VIA_GET_LAYER_COUNT
      data[1] = @layer_count
    when :ID_VIA_GET_BUFFER
      data = dynamic_keymap_get_buffer(data)
      unless @keymap_saved
        save_keymap
        @keymap_saved = true
      end
    when :ID_VIA_SET_BUFFER
      dynamic_keymap_set_buffer(data)
    else

    end

    return data
  end

  def via_get_layer_name(i)
    return :default if i==0
    
    s = "VIA_LAYER"+i.to_s
    return s.intern
  end

  def dynamic_keymap_get_buffer(data)
    offset = (data[1]<<8) | data[2]
    size   = data[3]
    layer_num = (offset/2/(cols_size * rows_size)).to_i
    key_index = (offset/2 - layer_num * (cols_size * rows_size)).to_i
    
    (size/2).times do |i|
      if key_index==(cols_size * rows_size)
        layer_num += 1
        key_index = 0
      end
      
      row = key_index / cols_size
      col = key_index % cols_size

      if @kbd.split
        row = key_index / ( cols_size/2 )
        col = key_index % ( cols_size/2 )
      end

      keycode = dynamic_keymap_get_keycode(layer_num, row, col)
      
      # @type var keycode : Integer
      data[i*2+4] = (keycode >> 8) & 0xFF
      data[i*2+5] = keycode & 0xFF
      
      key_index += 1
    end
    
    return data
  end

  def dynamic_keymap_set_buffer(data)
    # @type var data: Array[Integer]
    offset = (data[1]<<8) | data[2]
    size   = data[3]
    layer_num = ( offset/2/(cols_size * rows_size) ).to_i
    key_index = ( offset/2 - layer_num * (cols_size * rows_size) ).to_i
    
    (size/2).times do |i|
      keycode = data[i*2+4] << 8 | data[i*2+5]
      if key_index==(cols_size * rows_size)
        layer_num += 1
        key_index = 0
      end
      row = (key_index / rows_size).to_i
      col = (key_index % rows_size).to_i

      if @kbd.split
        row = key_index / ( cols_size/2 )
        col = key_index % ( cols_size/2 )
      end
      
      dynamic_keymap_set_keycode(layer_num, row, col, keycode);

      key_index += 1
    end
  end

  def dynamic_keymap_get_keycode(layer, row, col)
    if @kbd.split
      if row >= rows_size
        row -= rows_size
        col = cols_size - col - 1
      end
    end

    return @keymaps[layer][row][col] || 0
  end

  def dynamic_keymap_set_keycode(layer, row, col, keycode)
    if @kbd.split
      if row >= rows_size
        row -= rows_size
        col = cols_size - col - 1
      end
    end

    @keymaps[layer][row][col] = keycode
    @keymap_saved = false
  end
  
  def eval_val(script)
    if @kbd.sandbox.compile(script)
      if @kbd.sandbox.execute
        if @kbd.sandbox.wait && error = @kbd.sandbox.error
          return error.message
        else
          return @kbd.sandbox.result
        end
        @kbd.sandbox.suspend
      end
    else
      puts "Error: Compile failed"
      return nil
    end
  end

  def task
    if raw_hid_report_received?
      data = raw_hid_receive( get_last_received_raw_hid_report )
      # sleep is needed to be received
      sleep_ms 1
      report_raw_hid( data )
    end
  end
end
