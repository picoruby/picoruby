class MIDI
  attr_accessor :channel, :key_states, :bend_width, :bend_step, :bend_release_step

  NOTE_OFF_EVENT = 0x80
  NOTE_ON_EVENT  = 0x90
  PITCH_BEND_EVENT = 0xE0
  CONTROL_CHANGE_EVENT = 0xB0
  PROGRAM_CHANGE_EVENT = 0xC0

  MAX_EVENT = 64
  MAX_PITCHBEND_VALUE = 0x3FFF
  MIN_PITCHBEND_VALUE = 0
  DEFAULT_PITCHBEND_VALUE = 8192

  # MIDI Keycodes
  KEYCODE = {
    # MI_C:       0x01,
    # MI_Cs:      0x02,
    # MI_Db:      0x02,
    # MI_D:       0x03,
    # MI_Ds:      0x04,
    # MI_Eb:      0x04,
    # MI_E:       0x05,
    # MI_F:       0x06,
    # MI_Fs:      0x07,
    # MI_Gb:      0x07,
    # MI_G:       0x08,
    # MI_Gs:      0x09,
    # MI_Ab:      0x09,
    # MI_A:       0x0a,
    # MI_As:      0x0b,
    # MI_Bb:      0x0b,
    # MI_B:       0x0c,
    # MI_C1:      0x0d,
    # MI_Cs1:     0x0e,
    # MI_Db1:     0x0e,
    # MI_D1:      0x0f,
    # MI_Ds1:     0x10,
    # MI_Eb1:     0x10,
    # MI_E1:      0x11,
    # MI_F1:      0x12,
    # MI_Fs1:     0x13,
    # MI_Gb1:     0x13,
    # MI_G1:      0x14,
    # MI_Gs1:     0x15,
    # MI_Ab1:     0x15,
    # MI_A1:      0x16,
    # MI_As1:     0x17,
    # MI_Bb1:     0x17,
    # MI_B1:      0x18,
    MI_C2:      0x19,
    MI_Cs2:     0x1a,
    MI_Db2:     0x1a,
    MI_D2:      0x1b,
    MI_Ds2:     0x1c,
    MI_Eb2:     0x1c,
    MI_E2:      0x1d,
    MI_F2:      0x1e,
    MI_Fs2:     0x1f,
    MI_Gb2:     0x1f,
    MI_G2:      0x20,
    MI_Gs2:     0x21,
    MI_Ab2:     0x21,
    MI_A2:      0x22,
    MI_As2:     0x23,
    MI_Bb2:     0x23,
    MI_B2:      0x24,
    MI_C3:      0x25,
    MI_Cs3:     0x26,
    MI_Db3:     0x26,
    MI_D3:      0x27,
    MI_Ds3:     0x28,
    MI_Eb3:     0x28,
    MI_E3:      0x29,
    MI_F3:      0x2a,
    MI_Fs3:     0x2b,
    MI_Gb3:     0x2b,
    MI_G3:      0x2c,
    MI_Gs3:     0x2d,
    MI_Ab3:     0x2d,
    MI_A3:      0x2e,
    MI_As3:     0x2f,
    MI_Bb3:     0x2f,
    MI_B3:      0x30,
    MI_C4:      0x31,
    MI_Cs4:     0x32,
    MI_Db4:     0x32,
    MI_D4:      0x33,
    MI_Ds4:     0x34,
    MI_Eb4:     0x34,
    MI_E4:      0x35,
    MI_F4:      0x36,
    MI_Fs4:     0x37,
    MI_Gb4:     0x37,
    MI_G4:      0x38,
    MI_Gs4:     0x39,
    MI_Ab4:     0x39,
    MI_A4:      0x3a,
    MI_As4:     0x3b,
    MI_Bb4:     0x3b,
    MI_B4:      0x3c,
    # MI_C5:      0x3d,
    # MI_Cs5:     0x3e,
    # MI_Db5:     0x3e,
    # MI_D5:      0x3f,
    # MI_Ds5:     0x40,
    # MI_Eb5:     0x40,
    # MI_E5:      0x41,
    # MI_F5:      0x42,
    # MI_Fs5:     0x43,
    # MI_Gb5:     0x43,
    # MI_G5:      0x44,
    # MI_Gs5:     0x45,
    # MI_Ab5:     0x45,
    # MI_A5:      0x46,
    # MI_As5:     0x47,
    # MI_Bb5:     0x47,
    # MI_B5:      0x48,
    # MI_OCN2:    0x49, # Set octave to -2
    # MI_OCN1:    0x4a, # Set octave to -1
    MI_OC0:     0x4b, # Set octave to 0
    # MI_OC1:     0x4c, # Set octave to 1
    # MI_OC2:     0x4d, # Set octave to 2
    # MI_OC3:     0x4e, # Set octave to 3
    # MI_OC4:     0x4f, # Set octave to 4
    # MI_OC5:     0x50, # Set octave to 5
    # MI_OC6:     0x51, # Set octave to 6
    # MI_OC7:     0x52, # Set octave to 7
    MI_OCTD:    0x53, # Decrease octave
    MI_OCTU:    0x54, # Increase octave
    # MI_TRN6:    0x55, # Set transpose to -6 semitones
    # MI_TRN5:    0x56, # Set transpose to -5 semitones
    # MI_TRN4:    0x57, # Set transpose to -4 semitones
    # MI_TRN3:    0x58, # Set transpose to -3 semitones
    # MI_TRN2:    0x59, # Set transpose to -2 semitones
    # MI_TRN1:    0x5a, # Set transpose to -1 semitone
    MI_TR0:     0x5b, # No transposition
    # MI_TR1:     0x5c, # Set transpose to +1 semitone
    # MI_TR2:     0x5d, # Set transpose to +2 semitones
    # MI_TR3:     0x5e, # Set transpose to +3 semitones
    # MI_TR4:     0x5f, # Set transpose to +4 semitones
    # MI_TR5:     0x60, # Set transpose to +5 semitones
    # MI_TR6:     0x61, # Set transpose to +6 semitones
    MI_TRSD:    0x62, # Decrease transpose
    MI_TRSU:    0x63, # Increase transpose
    # MI_VL0:     0x64, # Set velocity to 0
    # MI_VL1:     0x65, # Set velocity to 12
    # MI_VL2:     0x66, # Set velocity to 25
    # MI_VL3:     0x67, # Set velocity to 38
    # MI_VL4:     0x68, # Set velocity to 51
    # MI_VL5:     0x69, # Set velocity to 64
    MI_VL6:     0x6a, # Set velocity to 76
    # MI_VL7:     0x6b, # Set velocity to 89
    # MI_VL8:     0x6c, # Set velocity to 102
    # MI_VL9:     0x6d, # Set velocity to 114
    # MI_VL10:    0x6e, # Set velocity to 127
    MI_VELD:    0x6f, # Decrease velocity
    MI_VELU:    0x70, # Increase velocity
    MI_CH1:     0x71, # Set channel to 1
    MI_CH2:     0x72, # Set channel to 2
    MI_CH3:     0x73, # Set channel to 3
    MI_CH4:     0x74, # Set channel to 4
    MI_CH5:     0x75, # Set channel to 5
    MI_CH6:     0x76, # Set channel to 6
    MI_CH7:     0x77, # Set channel to 7
    MI_CH8:     0x78, # Set channel to 8
    MI_CH9:     0x79, # Set channel to 9
    MI_CH10:    0x7a, # Set channel to 10
    MI_CH11:    0x7b, # Set channel to 11
    MI_CH12:    0x7c, # Set channel to 12
    MI_CH13:    0x7d, # Set channel to 13
    MI_CH14:    0x7e, # Set channel to 14
    MI_CH15:    0x7f, # Set channel to 15
    MI_CH16:    0x80, # Set channel to 16
    MI_CHND:    0x81, # Decrease channel
    MI_CHNU:    0x82, # Increase channel
    # MI_AOFF:    0x83, # Stop all notes
    # MI_SUST:    0x84, # Sustain
    # MI_PORT:    0x85, # Portamento
    # MI_SOST:    0x86, # Sostenuto
    # MI_SOFT:    0x87, # Soft pedal
    # MI_LEG:     0x88, # Legato
    # MI_MOD:     0x89, # Modulation
    # MI_MODD:    0x8a, # Decrease modulation speed
    # MI_MODU:    0x8b, # Increase modulation speed
    MI_BND:     0x8c, # Bend pitch down
    MI_BNDU:    0x8d, # Bend pitch up
    MI_PRG0:    0x8e, # Set program number to 0
    MI_PRGU:    0x8f, # Increase program number
    MI_PRGD:    0x90, # Decrease program number
  }

  VELOCITY_VALUES = [0, 12, 25, 38, 51, 64, 76, 89, 102, 114, 127]
  MAP_OFFSET = 0x800

  def self.keycode(sym)
    MIDI::KEYCODE[sym]
  end

  def self.keycode_from_mapcode(mapcode)
    mapcode - MIDI::MAP_OFFSET
  end

  def self.keycode_to_note_number(keycode)
    if (keycode >= 0x01 && keycode <= 0x48)
      keycode - 1
    else
      -1
    end
  end

  def initialize
    puts "Init MIDI"
    @channel = 0;
    @buffer = []
    @octave_offset = 0
    @transpose_offset = 0
    @velocity_offset = 6
    @program_no = 0
    @key_states = Hash.new
    @bend_width = 1365
    @bend_value = DEFAULT_PITCHBEND_VALUE
    @bend_step = 2000
  end

  # event
  #   press
  #   release
  # status
  #   released
  #   pressed
  #   wait_noteon
  #   wait_noteoff
  def update_event(keycode, event)
    if keycode >= 0x01 && keycode <= 0x48
      # for note
      case event
      when :press
        case @key_states[keycode]
        when :released, nil
          @key_states[keycode] = :wait_noteon
        end
      when :release
        case @key_states[keycode]
        when :pressed
          @key_states[keycode] = :wait_noteoff
        end
      when :noteon
        case @key_states[keycode]
        when :wait_noteon
          @key_states[keycode] = :pressed
        end
      when :noteoff
        case @key_states[keycode]
        when :wait_noteoff
          @key_states[keycode] = :released
        end
      end
    elsif keycode == KEYCODE[:MI_BND]
      # for pitch bend down
      case event
      when :press
        @key_states[KEYCODE[:MI_BND]] = :pressed
        press_bend_event(:down)
      when :release
        @key_states.delete(KEYCODE[:MI_BND])
        release_bend_event
      end
    elsif keycode == KEYCODE[:MI_BNDU]
      # for pitch bend up
      case event
      when :press
        @key_states[KEYCODE[:MI_BNDU]] = :pressed
        press_bend_event(:up)
      when :release
        @key_states.delete(KEYCODE[:MI_BNDU])
        release_bend_event
      end
    else
      # for control
      case event
      when :press
        case @key_states[keycode]
        when nil
          @key_states[keycode] = :wait_request
        end
      when :release
        case @key_states[keycode]
        when :pressed
          @key_states.delete(keycode)
        end
      when :do_request
        case @key_states[keycode]
        when :wait_request
          @key_states[keycode] = :pressed
        end
      end
    end
  end

  def process_request(keycode, params)
    case keycode
    when KEYCODE[:MI_OC0]
      @octave_offset = 0
    when KEYCODE[:MI_OCTD]
      return if @octave_offset - 1 < -2
      @octave_offset -= 1
    when KEYCODE[:MI_OCTU]
      return if @octave_offset + 1 > 7
      @octave_offset += 1
    when KEYCODE[:MI_TR0]
      @transpose_offset = 0
    when KEYCODE[:MI_TRSD]
      return if @transpose_offset - 1 < -6
      @transpose_offset -= 1
    when KEYCODE[:MI_TRSU]
      return if @transpose_offset + 1 > 6
      @transpose_offset += 1
    when KEYCODE[:MI_VL6]
      @velocity_offset = 6
    when KEYCODE[:MI_VELD]
      return if @velocity_offset - 1 < 0
      @velocity_offset -= 1
    when KEYCODE[:MI_VELU]
      return if @velocity_offset + 1 > 10
      @velocity_offset += 1
    when (KEYCODE[:MI_CH1]..KEYCODE[:MI_CH16])
      @channel = keycode - 0x71
      puts "set channel = #{@channel}"
    when KEYCODE[:MI_PRG0]
      @program_no = 0
      send_pc(@program_no)
      puts "set program_no = #{@program_no}"
    when KEYCODE[:MI_PRGD]
      @program_no = (@program_no - 1) < 0 ? (@program_no + 128 - 1) : @program_no - 1
      send_pc(@program_no)
      puts "set program_no = #{@program_no}"
    when KEYCODE[:MI_PRGU]
      @program_no = (@program_no + 1) > 128 ? (@program_no + 1 - 128) : @program_no + 1
      send_pc(@program_no)
      puts "set program_no = #{@program_no}"
    end
  end

  def note_on(number, velocity)
    velocity ||= VELOCITY_VALUES[@velocity_offset]
    puts "note-on channel: #{@channel}, number: #{number}, velocity: #{velocity}"
    return if number < 0 || number > 128
    return if velocity < 0 || velocity > 128
    modified_note = number + @octave_offset * 12 + @transpose_offset
    @buffer << [NOTE_ON_EVENT | @channel, modified_note, velocity] if @buffer.length <= 64
  end

  def note_off(number, velocity)
    velocity ||= VELOCITY_VALUES[@velocity_offset]
    puts "note-off channel: #{@channel}, number: #{number}, velocity: #{velocity}"
    return if number < 0 || number > 128
    return if velocity < 0 || velocity > 128
    
    modified_note = number + @octave_offset * 12 + @transpose_offset
    @buffer << [NOTE_OFF_EVENT | @channel, modified_note, velocity] if @buffer.length <= 64
  end

  def send_pitchbend
    puts "send-pitchbend channel: #{@channel}, value: #{@bend_value}"
    @buffer << [PITCH_BEND_EVENT | @channel, @bend_value & 0x7f, (@bend_value >> 7) & 0x7f] if @buffer.length <= 64
  end

  def send_pc(number)
    puts "send-pc channel: #{@channel}, number: #{number}"
    return if number < 0 || number > 128
    @buffer << [PROGRAM_CHANGE_EVENT | @channel, number] if @buffer.length <= 64

  end

  def send_cc(number, value)
    puts "send-cc channel: #{@channel}, number: #{number}, value: #{value}"
    return if number < 0 || number > 128
    return if value < 0 || value > 128
    @buffer << [CONTROL_CHANGE_EVENT | @channel, number, value]
  end

  def task
    MIDI.init
    @key_states.each do |keycode, state|
      case state
      when :wait_noteon
        note_on(MIDI.keycode_to_note_number(keycode), nil)
        update_event(keycode, :noteon)
      when :wait_noteoff
        note_off(MIDI.keycode_to_note_number(keycode), nil)
        update_event(keycode, :noteoff)
      when :wait_request
        process_request(keycode, {})
        update_event(keycode, :do_request)
      end
    end

    @buffer.each do |ev|
      MIDI.write(ev)
    end
    @buffer.clear
  end

  def press_bend_event(up_or_down)
    if up_or_down == :up
      if @bend_value + @bend_step > MAX_PITCHBEND_VALUE
        @bend_value = MAX_PITCHBEND_VALUE
      else
        @bend_value += @bend_step
      end
    elsif up_or_down == :down
      if @bend_value - @bend_step < MIN_PITCHBEND_VALUE
        @bend_value = MIN_PITCHBEND_VALUE
      else
        @bend_value -= @bend_step
      end
    end
    send_pitchbend
  end

  def release_bend_event
    @bend_value = DEFAULT_PITCHBEND_VALUE
  end
end