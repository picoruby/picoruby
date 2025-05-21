#!/usr/bin/env ruby

# ------------------------------------------------------------
# Utility to convert Shinonome BDF fonts into C arrays
#   - ASCII   : Stores characters 0x20–0x7E as a flat 1D array
#   - JISX208 : Organized by ku-ten (row-cell) into 2D arrays with a pointer table
# ------------------------------------------------------------

require "pathname"

module FontConvert
  module Util
    def self.bytes_per_char(w, h)
      ((w * h) + 7) / 8
    end

    # "01010101..."-> ["0x55", "0xAA", ...]
    def self.bin_to_hex_bytes(bin_str)
      bin_str.chars.each_slice(8).map do |bits|
        "0x#{bits.join.to_i(2).to_s(16).rjust(2, '0')}"
      end
    end
  end

  class BdfFont
    Char = Struct.new(:code_hex, :bitmap_bits) # "0041", "0100..."

    def initialize(name, path, width, height)
      @name = name
      @path, @w, @h = Pathname(path), width, height
      @chars = parse_bdf
    end
    attr_reader :chars, :name

    private

    def parse_bdf
      lines = @path.readlines(chomp: true)
      i      = 0
      chars  = []

      while i < lines.size
        unless lines[i].start_with?("STARTCHAR")
          i += 1
          next
        end

        # ------ ENCODING ---------
        enc_line = lines[i + 1..].find { _1.start_with?("ENCODING") }
        code_dec = enc_line.split[1].to_i
        code_hex = "%04X" % code_dec

        # ------ BITMAP ------
        bitmap_start = lines[i..].index { _1 == "BITMAP" }
        raise "BITMAP not found for #{code_hex}" unless bitmap_start
        bitmap_start += i + 1
        bitmap_end   = bitmap_start + @h - 1

        bitmap_rows = []
        (bitmap_start..bitmap_end).each do |ln|
          row_hex   = lines[ln].strip
          need_hex  = (@w + 3) / 4
          row_hex   = row_hex.ljust(need_hex, "0")[0, need_hex]
          row_bits  = row_hex.to_i(16).to_s(2).rjust(need_hex * 4, "0")[0, @w]
          bitmap_rows << row_bits
        end

        chars << Char.new(code_hex, bitmap_rows.join)

        # ------- Skip until ENDCHAR ---------
        endchar = lines[i..].index("ENDCHAR")
        i += endchar + 1
      end
      chars
    end
  end

  class AsciiTable
    RANGE = (0x20..0x7E).freeze

    def initialize(font)
      @font   = font
      @bytes  = Util.bytes_per_char(font.instance_variable_get(:@w),
                                    font.instance_variable_get(:@h))
    end

    def to_c
      body = RANGE.map do |cp|
        char  = @font.chars.find { _1.code_hex.to_i(16) == cp }
        bits  = char ? char.bitmap_bits : "1" * (@bytes * 8) # undef: FF
        line  = Util.bin_to_hex_bytes(bits).join(", ")
        "  #{line}, // 0x%02X" % cp
      end.join("\n")

      <<~C
        /* ---------- ASCII (#{@bytes} bytes/char) ---------- */
        #include <stdint.h>
        const uint8_t ascii_#{@font.name}[#{RANGE.size * @bytes}] = {
        #{body}
        };
      C
    end
  end

  class JisTable
    KU_MAP = begin
      h = {}
      (0x21..0x2F).each_with_index { |fb, i| h[fb] = i + 1  } # 区 1–15
      (0x30..0x4F).each_with_index { |fb, i| h[fb] = i + 16 } # 区16–47
      h
    end.freeze

    def initialize(font)
      @font  = font
      @name  = font.name
      @bytes = Util.bytes_per_char(font.instance_variable_get(:@w),
                                  font.instance_variable_get(:@h))
    end

    def to_c
      arrays = KU_MAP.map { |fb, ku| ku_array(fb, ku) }.join("\n")
      ptrtbl = ptr_table

      <<~C
        #include <stdint.h>
        /* ---------- JIS X 0208 (#{@bytes} bytes/char) ---------- */
        #{arrays}
        #{ptrtbl}
      C
    end

    private

    def ku_array(firstbyte, ku)
      fb_hex = "%02X" % firstbyte
      lines  = (0x21..0x7E).map do |sb|
        code_hex = "%02X%02X" % [firstbyte, sb]
        char  = @font.chars.find { _1.code_hex == code_hex }
        bits  = char ? char.bitmap_bits : "1" * (@bytes * 8)
        "  #{Util.bin_to_hex_bytes(bits).join(', ')}, // #{code_hex}"
      end.join("\n")

      <<~C
        const uint8_t jis208_#{@name}_#{fb_hex}[#{94 * @bytes}] = { // ku:#{ku}, firstbyte:#{fb_hex}
        #{lines}
        };
      C
    end

    def ptr_table
      rows = (0x21..0x4F).map do |fb|
        fb_hex = "%02X" % fb
        line   = KU_MAP.key?(fb) ? "  jis208_#{@name}_#{fb_hex}," : "  NULL,"
        line
      end.join("\n")

      <<~C
        const uint8_t *jis208_#{@name}[] = {
        #{rows}
        };
      C
    end
  end

  def self.make(f)
    puts "Building #{f[:dst]}"
    font  = BdfFont.new(f[:name], f[:src], f[:w], f[:h])
    table = (f[:type] == :ascii ? AsciiTable : JisTable).new(font)
    File.write(f[:dst], table.to_c)
  end
end

