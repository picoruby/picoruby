# PRS stands for PicoRuby Sound format.
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
  OP_SET_PAN    = 0x30 # uint16_t: (OP & track), pan
  OP_SET_TIMBRE = 0x40 # uint16_t: (OP & track), timbre
  OP_SET_LFO    = 0x50 # uint32_t: OP, track, depth, rate
  OP_WAIT       = 0x60 # uint32_t: OP, (0x000000..0xFFFFFF)
  SONGNAME_MAX_LEN = 16

  def self.check_header(header)
    if header.nil? || header.size < HEADER_SIZE
      raise "Invalid PRS file header size"
    end
    len1, len2, len3, len4 = header[LENGTH_OFFSET, 4]&.bytes
    if len1.nil? || len2.nil? || len3.nil? || len4.nil?
      raise "Invalid PRS file length bytes"
    end
    length = (len1 & 0xFF) | (len2 << 8) | (len3 << 16) | (len4 << 24)
    unless header&.start_with?(SIGNATURE)
      raise "Invalid PRS signature in file"
    end
    loop1, loop2, loop3, loop4 = header[LOOP_START_OFFSET, 4]&.bytes
    if loop1.nil? || loop2.nil? || loop3.nil? || loop4.nil?
      raise "Invalid PRS file loop start bytes"
    end
    loop_start_pos = (loop1 & 0xFF) | (loop2 << 8) | (loop3 << 16) | (loop4 << 24)
    return [length, loop_start_pos]
  end


  class Compiler
    def self.save(tracks, filename, songname: "")
      compiler = self.new
      compiler.tracks = tracks
      compiler.songname = songname
      compiler.save_to(filename)
    end

    attr_accessor :tracks
    attr_reader :songname

    def initialize
      @length = 0
      @file = nil
      @tracks = []
      @songname = "[Untitled]"
    end

    def songname=(name)
      @songname = name[0, SONGNAME_MAX_LEN]
    end

    def save_to(filename)
      @file = File.open(filename, "w+")
      @file.write(SIGNATURE)
      @file.write(VERSION)
      @file.write(@tracks.size.chr)
      @file.write(RESERVED)
      @file.write("\0\0\0\0") # Placeholder for length
      @file.write("\0\0\0\0") # Placeholder for loop start position
      @file.write(@songname.ljust(SONGNAME_MAX_LEN, "\0"))
      mixer = 0b111000 # Noise all off, Tone all on
      chip_clock = PSG::Driver::CHIP_CLOCK
      loop_start_pos = 0
      MML.compile_multi(@tracks) do |delta, tr, command, *args|
        if 0 < delta
          gen4(OP_WAIT, 0, delta & 0xFF, (delta >> 8) & 0xFF, (delta >> 16) & 0xFF)
        end
        case command
        when :segno
          loop_start_pos = @length + HEADER_SIZE
        when :mute
          gen2(OP_MUTE, tr, args[0])
        when :play
          gen2(OP_SEND_REG, tr * 2, (chip_clock / (32 * args[0])).to_i & 0xFF)
          gen2(OP_SEND_REG, tr * 2 + 1, ((chip_clock / (32 * args[0])).to_i >> 8) & 0x0F)
        when :rest
          gen2(OP_SEND_REG, tr * 2, 0)
          gen2(OP_SEND_REG, tr * 2 + 1, 0)
        when :volume
          gen2(OP_SEND_REG, tr + 8, args[0])
        when :env_period
          gen2(OP_SEND_REG, 11, args[0] & 0xFF)
          gen2(OP_SEND_REG, 12, (args[0] >> 8) & 0xFF)
        when :env_shape
          gen2(OP_SEND_REG, 13, args[0])
        when :timbre
          gen2(OP_SET_TIMBRE, tr, args[0])
        when :pan
          gen2(OP_SET_PAN, tr, args[0])
        when :lfo
          gen4(OP_SET_LFO, 0, tr, args[0], args[1])
        when :mixer
          case args[0]
          when 0 # Tone on, Noise off
            mixer |= (1 << (tr + 3))  # Set noise bit (off)
            mixer &= ~(1 << tr)       # Clear tone bit (on)
          when 1 # Tone off, Noise on
            mixer &= ~(1 << (tr + 3)) # Clear noise bit (on)
            mixer |= (1 << tr)        # Set tone bit (off)
          when 2 # Tone on, Noise on
            mixer &= ~(1 << tr)       # Clear tone bit (on)
            mixer &= ~(1 << (tr + 3)) # Clear noise bit (on)
          end
          gen2(OP_SEND_REG, 7, mixer)
        when :noise
          gen2(OP_SEND_REG, 6, args[0])
        end
      end
      @file.seek(LENGTH_OFFSET)
      @file.write (@length & 0xFF).chr
      @file.write ((@length >> 8) & 0xFF).chr
      @file.write ((@length >> 16) & 0xFF).chr
      @file.write ((@length >> 24) & 0xFF).chr
      @file.seek(LOOP_START_OFFSET)
      @file.write (loop_start_pos & 0xFF).chr
      @file.write ((loop_start_pos >> 8) & 0xFF).chr
      @file.write ((loop_start_pos >> 16) & 0xFF).chr
      @file.write ((loop_start_pos >> 24) & 0xFF).chr
    ensure
      @file&.close
    end

    # private

    def gen2(op, reg, value)
      code = "#{((op|(reg&0xF)) & 0xFF).chr}#{(value & 0xFF).chr}"
      @file.write code
      @length += code.size
    end

    def gen4(op, reg, val1, val2, val3)
      code = "#{((op|(reg&0xF)) & 0xFF).chr}#{(val1 & 0xFF).chr}#{(val2 & 0xFF).chr}#{(val3 & 0xFF).chr}"
      @file.write code
      @length += code.size
    end

  end

end

