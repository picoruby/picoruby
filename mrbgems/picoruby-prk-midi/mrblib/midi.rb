if RUBY_ENGINE == 'mruby/c'
  require "mml2midi"
end

class MIDI
  attr_accessor :channel, :key_states, :bend_width, :bend_step,
    :default_bpm, :arpeggiate_mode, :chord_mode, :chord_pattern,
    :slots, :play_repeat

  NOTE_OFF_EVENT = 0x80
  NOTE_ON_EVENT  = 0x90
  PITCH_BEND_EVENT = 0xE0
  CONTROL_CHANGE_EVENT = 0xB0
  PROGRAM_CHANGE_EVENT = 0xC0

  SRT_TIMING_CLOCK = 0xF8
  SRT_START = 0xFA
  SRT_STOP = 0xFB

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
    MI_AOFF:    0x83, # Stop all notes
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
    MI_CRDTGL:  0x91, # Toggle chord mode
    MI_NEXTCRD: 0x92, # Change next chord pattern
    MI_PREVCRD: 0x93, # Change previous chord pattern
    MI_ARPGTGL: 0x94, # Toggle arppegiate mode
    MI_ARPGON:  0x95, # Arppegiate mode on
    MI_ARPGOFF: 0x96, # Arppegiate mode off
    MI_SRTSTART:0x97, # Start MIDI System Realtime
    MI_SRTSTOP: 0xa1, # Stop MIDI System Realtime
    MI_BPMUP:   0xa2, # Insurace BPM
    MI_BPMDOWN: 0xa3, # Decreace BPM
    MI_BPMDFLT: 0xa4, # Reset BPM to default(or 120)
    MI_PL1GL1:  0xb0, # Play slot 1, group 1
    MI_PL1GL2:  0xb1, # Play slot 1, group 2
    MI_PL1GL3:  0xb2, # Play slot 1, group 3
    MI_PL2GL1:  0xb3, # Play slot 2, group 1
    MI_PL2GL2:  0xb4, # Play slot 2, group 2
    MI_PL2GL3:  0xb5, # Play slot 2, group 3
    MI_PL3GL1:  0xb6, # Play slot 3, group 1
    MI_PL3GL2:  0xb7, # Play slot 3, group 2
    MI_PL3GL3:  0xb8, # Play slot 3, group 3
    MI_PL4GL1:  0xb9, # Play slot 3, group 1
    MI_PL4GL2:  0xc0, # Play slot 3, group 2
    MI_PL4GL3:  0xc1, # Play slot 3, group 3
    MI_PL_RPT:  0xc2, # Play with repeat
  }

  CHORD_PATTERNS = {
    # Major chords
    major: [1, 5, 8],
    major7th: [1, 5, 8, 12],
    major9th: [1, 5, 8, 12, 15],
    major11th: [1, 5, 8, 12, 15, 19],
    major13th: [1, 5, 8, 12, 15, 19, 22],

    # Minor chords
    minor: [1, 4, 8],
    minor7th: [1, 4, 8, 11],
    minor9th: [1, 4, 8, 11, 14],
    minor11th: [1, 4, 8, 11, 14, 18],
    minor13th: [1, 4, 8, 11, 14, 18, 21],

    # Dominant chords
    dominant7th: [1, 5, 8, 10],
    dominant9th: [1, 5, 8, 10, 14],
    dominant11th: [1, 5, 8, 10, 14, 17],
    dominant13th: [1, 5, 8, 10, 14, 17, 21],

    # Diminished chords
    diminished: [1, 4, 7],
    diminished7th: [1, 4, 7, 10],

    # Augmented chords
    augmented: [1, 5, 9],
    augmented7th: [1, 5, 9, 13],
  }

  VELOCITY_VALUES = [0, 12, 25, 38, 51, 64, 76, 89, 102, 114, 127]
  MAP_OFFSET = 0x800
  MAX_SLOTS = 4
  MAX_SLOT_GROUPS = 3

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
    @channel = 0
    @buffer = []
    @octave_offset = 0
    @transpose_offset = 0
    @velocity_offset = 6
    @program_no = 0
    @key_states = Hash.new
    @bend_width = 1365
    @bend_value = DEFAULT_PITCHBEND_VALUE
    @bend_step = 2000
    @default_bpm = 120
    @bpm = @default_bpm
    @arpeggiate_mode = false
    @chord_pattern = :major
    @chord_mode = false
    @play_status = :stop
    @previous_clock_time = nil
    @previous_sixteenth_beat_time = nil
    @duration_per_beat = -1
    @duration_per_sixteenth_beat = -1
    @duration_per_clock = -1
    @slots = Array.new(MAX_SLOTS)

    @slots.each_with_index do |slot, i|
      @slots[i] = Array.new(MAX_SLOT_GROUPS)
    end

    @current_seq_no = 0
    @current_slot_group = Array.new(MAX_SLOTS)
    @start_seq_no = Array.new(MAX_SLOTS)
    @is_played = Array.new(MAX_SLOTS)
    @play_repeat = false
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
    elsif keycode == KEYCODE[:MI_BPMUP]
      # for increace bpm
      case event
      when :press
        @key_states[KEYCODE[:MI_BPMUP]] = :pressed
        change_bpm(@bpm + 1)
      end
    elsif keycode == KEYCODE[:MI_BPMDOWN]
      # for decreace bpm
      case event
      when :press
        @key_states[KEYCODE[:MI_BPMDOWN]] = :pressed
        change_bpm(@bpm - 1)
      end
    else
      # for control, chord mode, arpeggiate mode
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
    when KEYCODE[:MI_AOFF]
      (0..128).each{|n| note_off(n, nil) }
    when KEYCODE[:MI_CRDTGL]
      @chord_mode = !@chord_mode
      puts "set chord mode #{@chord_mode}"
    when KEYCODE[:MI_NEXTCRD], KEYCODE[:MI_PREVCRD]
      if (index = CHORD_PATTERNS.keys.index(@chord_pattern))
        if keycode == KEYCODE[:MI_NEXTCRD]
          @chord_pattern = CHORD_PATTERNS.keys[(index + 1) % CHORD_PATTERNS.count]
        elsif keycode == KEYCODE[:MI_PREVCRD]
          @chord_pattern = CHORD_PATTERNS.keys[(index - 1) % CHORD_PATTERNS.count]
        end
        puts "set chord_pattern = #{@chord_pattern}"
      else
        puts "unknown chord_pattern = #{@chord_pattern}"
      end
    when KEYCODE[:MI_ARPGTGL]
      @arpeggiate_mode = !@arpeggiate_mode
    when KEYCODE[:MI_ARPGON]
      @arpeggiate_mode = true
    when KEYCODE[:MI_ARPGOFF]
      @arpeggiate_mode = false
    when KEYCODE[:MI_BPMDFLT]
      change_bpm(@default_bpm)
    when KEYCODE[:MI_SRTSTART]
      change_bpm(@bpm)
      send_srt_start
    when KEYCODE[:MI_SRTSTOP]
      send_srt_stop
    end
  end

  def play_slot(params)
    puts "play_slot: slot: #{params[:slot_no]}, group: #{params[:group_no]}"
    @start_seq_no[params[:slot_no]] = @current_seq_no 
    @current_slot_group[params[:slot_no]] = params[:group_no]
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

  def send_srt_clock
    if @play_status == :start
      current_time = Machine.board_millis
      duration = current_time - (@previous_clock_time || 0)

      if @previous_clock_time.nil?
        @buffer << [SRT_TIMING_CLOCK]
        @previous_clock_time = current_time
        # puts "send_srt_clock duration: #{duration}ms"
      else
        cnt = duration / @duration_per_clock
        if cnt > 0
          cnt.times do
            @buffer << [SRT_TIMING_CLOCK]
          end
          @previous_clock_time = current_time - duration % @duration_per_clock
        end
        # puts "send_srt_clock #{cnt}times duration: #{duration}ms"
      end
    end
  end

  def calc_seq_no
    if @play_status == :start
      current_time = Machine.board_millis
      (0...MAX_SLOTS).each{|slot| @is_played[slot] = false }
      if @previous_sixteenth_beat_time.nil?
        @current_seq_no = 0
        @previous_sixteenth_beat_time = current_time
      else
        if current_time - @previous_sixteenth_beat_time.to_i >= @duration_per_sixteenth_beat
          @current_seq_no += 1
          @previous_sixteenth_beat_time = current_time
        end
      end
    end
  end

  def send_srt_start
    if @play_status == :stop
      puts "send_srt_start"
      @buffer << [SRT_START]
      @play_status = :start
      @previous_clock_time = nil
    end
  end

  def send_srt_stop
    if @play_status == :start
      puts "send_srt_stop"
      @buffer << [SRT_STOP]
      @play_status = :stop
      @previous_clock_time = nil
    end
  end

  def task
    MIDI.init

    calc_seq_no
    send_srt_clock

    @key_states.each do |keycode, state|
      case state
      when :wait_noteon
        note_num = MIDI.keycode_to_note_number(keycode)
        if @chord_mode
          convert_chord_pattern(note_num).each do |note|
            note_on(note, nil)
          end
        else
          note_on(note_num, nil)
        end
        update_event(keycode, :noteon)
      when :wait_noteoff
        note_num = MIDI.keycode_to_note_number(keycode)
        if @chord_mode
          convert_chord_pattern(note_num).each do |note|
            note_off(note, nil)
          end
        else
          note_off(note_num, nil)
        end
        update_event(keycode, :noteoff)
      when :wait_request
        if keycode >= KEYCODE[:MI_PL1GL1] && keycode <= (KEYCODE[:MI_PL1GL1] + MAX_SLOTS * MAX_SLOT_GROUPS - 1)
          rel = keycode - KEYCODE[:MI_PL1GL1]
          play_slot(slot_no: rel / MAX_SLOT_GROUPS, group_no: rel % MAX_SLOT_GROUPS)
        else
          process_request(keycode, {})
        end
        update_event(keycode, :do_request)
      end
    end

    process_slots

    @buffer.each do |ev|
      MIDI.write(ev)
    end
    @buffer.clear
  end

  def process_slots
    if @play_status == :start
      (0..MAX_SLOTS).each do |slot|
        start_seq_no = @start_seq_no[slot]
        current_slot_group = @current_slot_group[slot]
        next if @is_played[slot] || start_seq_no.nil? || current_slot_group.nil?
        if start_seq_no >= 0 && !current_slot_group.nil? &&
          seq_no = @current_seq_no - start_seq_no
          if @slots[slot] && @slots[slot][current_slot_group] && @slots[slot][current_slot_group][seq_no]
            @slots[slot][current_slot_group][seq_no]&.each do |notes|
              @buffer << notes
            end
            @is_played[slot] = true
          end
        end
      end
    end
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

  def convert_chord_pattern(root_note)
    if CHORD_PATTERNS[@chord_pattern]
      CHORD_PATTERNS[@chord_pattern].map{|n| root_note + n - 1 }
    else
      [root_note]
    end
  end

  def change_bpm(bpm)
    return if bpm < 20 || bpm > 300
    @bpm = bpm
    @duration_per_beat = 60000 / @bpm
    @duration_per_sixteenth_beat = @duration_per_beat / 4
    @duration_per_clock = (@duration_per_beat || 0) / 24
    puts "set BPM = #{@bpm}(#{@duration_per_beat}ms/beat)"
  end

  # For insert note from MML
  def add_song(slot_no, slot_group_no, ch, *measures)
    return if slot_no.nil? || slot_group_no.nil?
    @slots[slot_no] ||= []
    @slots[slot_no][slot_group_no] ||= []
    notes = @slots[slot_no][slot_group_no]
    notes.clear

    velocity = VELOCITY_VALUES[@velocity_offset]
    MML2MIDI.new.compile(measures.join) do |seq, params|
      case params[0]
      when :tempo
        change_bpm(params[1])
      when :note
        notes[seq] ||= []
        notes[seq] << [NOTE_ON_EVENT | ch, params[1], velocity]
        notes[seq + params[2]] ||= []
        notes[seq + params[2]] << [NOTE_OFF_EVENT | ch, params[1], 0]
      when :ch
        ch = params[1]
      when :prg_no
        notes[seq] ||= []
        notes[seq] << [PROGRAM_CHANGE_EVENT | ch, params[1]]
      end
    end
  end
end
