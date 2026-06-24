# PSG::PRS stands for PicoRuby Sound format.
# - Binary format for sound data for PSG::Driver
# - Conceptually similar to NSF (Nintendo Sound Format)
# - Consists of a header and a sequence of commands
# - File name: "*.prs"
#
# Header 32 bytes:
# - uint32_t: "PRS\0" (signature)
# - uint16_t: version (eg. "\0\1")
# - uint8_t : number of tracks (0-15)
# - uint8_t : reserved (0)
# - uint32_t: length of the data in bytes, doen't include header
# - uint32_t: loop start position. 0 means no loop
# - 16 bytes: Song name, padded with null bytes to 16 bytes
module PSG
  module PRS
    SIGNATURE = "PRS\0"
    VERSION = "\0\1"
    NUMBER_OF_TRACKS = "\0"
    RESERVED = "\0"
    LENGTH_OFFSET = SIGNATURE.size + VERSION.size + NUMBER_OF_TRACKS.size + RESERVED.size
    LOOP_START_OFFSET = LENGTH_OFFSET + 4
    HEADER_SIZE = 32
    OP_MUTE       = 0x10 # uint16_t: (OP & track), flag
    OP_SEND_REG   = 0x20 # uint16_t: (OP & reg),   value
    OP_SET_LEGATO = 0x30 # uint16_t: (OP & track), legato
    OP_SET_PAN    = 0x40 # uint16_t: (OP & track), pan
    OP_SET_TIMBRE = 0x50 # uint16_t: (OP & track), timbre
    OP_SET_LFO    = 0x60 # uint32_t: OP, track, depth, rate
    OP_WAIT       = 0x70 # uint32_t: OP, (0x000000..0xFFFFFF)
    SONGNAME_MAX_LEN = 16

    def self.check_header(header)
      if header.nil? || header.bytesize < HEADER_SIZE
        raise "Invalid PRS file header size"
      end
      len1 = header.getbyte(LENGTH_OFFSET)     || 0
      len2 = header.getbyte(LENGTH_OFFSET + 1) || 0
      len3 = header.getbyte(LENGTH_OFFSET + 2) || 0
      len4 = header.getbyte(LENGTH_OFFSET + 3) || 0
      length = (len1 & 0xFF) | (len2 << 8) | (len3 << 16) | (len4 << 24)
      unless header&.start_with?(SIGNATURE)
        raise "Invalid PRS signature in file"
      end
      loop1 = header.getbyte(LOOP_START_OFFSET)     || 0
      loop2 = header.getbyte(LOOP_START_OFFSET + 1) || 0
      loop3 = header.getbyte(LOOP_START_OFFSET + 2) || 0
      loop4 = header.getbyte(LOOP_START_OFFSET + 3) || 0
      loop_start_pos = (loop1 & 0xFF) | (loop2 << 8) | (loop3 << 16) | (loop4 << 24)
      return [length, loop_start_pos]
    end


    class Compiler
      def self.save(sequence, filename, songname: "")
        compiler = self.new(sequence)
        compiler.songname = songname
        compiler.save_to(filename)
      end

      attr_reader :sequence
      attr_reader :songname

      def initialize(sequence)
        @length = 0
        @file = nil
        @sequence = sequence
        @songname = "[Untitled]"
        @tempo = 120
        @volume = [15, 15, 15]
        @bend = [8192, 8192, 8192]
        @bend_range = [2, 2, 2]
        @rpn_msb = [127, 127, 127]
        @rpn_lsb = [127, 127, 127]
      end

      def songname=(name)
        @songname = name[0, SONGNAME_MAX_LEN]
      end

      def save_to(filename)
        @file = File.open(filename, "w+")
        @file.write(SIGNATURE)
        @file.write(VERSION)
        @file.write([3].pack("C"))
        @file.write(RESERVED)
        @file.write("\0\0\0\0") # Placeholder for length
        @file.write("\0\0\0\0") # Placeholder for loop start position
        @file.write(@songname.ljust(SONGNAME_MAX_LEN, "\0"))
        mixer = 0b111000
        loop_start_pos = 0
        @sequence.reset
        while (item = @sequence.next_event)
          delta_ticks = item[0]
          event = item[1]
          delta_ms = delta_ticks * 60_000 / (@sequence.ppqn * @tempo)
          if 0 < delta_ms
            gen4(OP_WAIT, 0, delta_ms & 0xFF, (delta_ms >> 8) & 0xFF, (delta_ms >> 16) & 0xFF)
          end
          command = event[0]
          channel = event[1]
          voice = channel.is_a?(Integer) ? channel : 0
          if (MIDIBASE::CHANNEL_EVENTS.include?(command) || command == :psg) && 3 <= voice
            raise ArgumentError, "PRS supports MIDI channels 0..2"
          end
          case command
          when :tempo
            @tempo = event[1]
          when :loop_point
            loop_start_pos = @length + HEADER_SIZE
          when :note_on
            period = bent_period(event[2], voice)
            gen2(OP_SEND_REG, voice * 2, period & 0xFF)
            gen2(OP_SEND_REG, voice * 2 + 1, period >> 8 & 0x0F)
            gen2(OP_SEND_REG, voice + 8, @volume[voice])
            gen2(OP_MUTE, voice, 0)
          when :note_off
            gen2(OP_MUTE, voice, 1)
          when :program_change
            gen2(OP_SET_TIMBRE, voice, event[2] & 0x03)
          when :pitch_bend
            @bend[voice] = event[2]
          when :control_change
            controller = event[2]
            value = event[3]
            if controller == 7
              @volume[voice] = (value * 15 + 63) / 127
              gen2(OP_SEND_REG, voice + 8, @volume[voice])
            elsif controller == 10
              gen2(OP_SET_PAN, voice, (value * 15 + 63) / 127)
            elsif controller == 68
              gen2(OP_SET_LEGATO, voice, 64 <= value ? 1 : 0)
            elsif controller == 101
              @rpn_msb[voice] = value
            elsif controller == 100
              @rpn_lsb[voice] = value
            elsif controller == 6 && @rpn_msb[voice] == 0 && @rpn_lsb[voice] == 0
              @bend_range[voice] = value
            end
          when :psg
            case event[2]
            when :env_period
              gen2(OP_SEND_REG, 11, event[3] & 0xFF)
              gen2(OP_SEND_REG, 12, event[3] >> 8 & 0xFF)
            when :env_shape
              gen2(OP_SEND_REG, 13, event[3])
              gen2(OP_SEND_REG, voice + 8, 16)
            when :lfo
              gen4(OP_SET_LFO, 0, voice, event[3], event[4])
            when :noise
              gen2(OP_SEND_REG, 6, event[3])
            when :mixer
              mode = event[3]
              if mode == 0
                mixer |= 1 << (voice + 3)
                mixer &= ~(1 << voice)
              elsif mode == 1
                mixer &= ~(1 << (voice + 3))
                mixer |= 1 << voice
              elsif mode == 2
                mixer &= ~(1 << voice)
                mixer &= ~(1 << (voice + 3))
              end
              gen2(OP_SEND_REG, 7, mixer)
            end
          end
        end
        @file.seek(LENGTH_OFFSET)
        @file.write [@length].pack("V")
        @file.seek(LOOP_START_OFFSET)
        @file.write [loop_start_pos].pack("V")
      ensure
        @file&.close
      end

      private def bent_period(note, voice)
        semitones = (@bend[voice] - 8192) * @bend_range[voice].to_f / 8192.0
        PSG.note_to_period(note + semitones)
      end

      private def gen2(op, reg, value)
        code = [(op|(reg&0xF)) & 0xFF, value & 0xFF].pack("CC")
        @file.write code
        @length += code.size
      end

      private def gen4(op, reg, val1, val2, val3)
        code = [(op|(reg&0xF)) & 0xFF, val1 & 0xFF, val2 & 0xFF, val3 & 0xFF].pack("CCCC")
        @file.write code
        @length += code.size
      end

    end

  end
end
