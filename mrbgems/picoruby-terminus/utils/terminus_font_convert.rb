#!/usr/bin/env ruby

# ------------------------------------------------------------
# Utility to convert Terminus BDF fonts into C arrays
#   - Simple ASCII only implementation for fixed-width fonts
# ------------------------------------------------------------

require "pathname"

module TerminusFontConvert
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
    attr_reader :chars, :name, :w, :h

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
      @bytes  = Util.bytes_per_char(font.w, font.h)
    end

    def to_c
      body = RANGE.map do |cp|
        char  = @font.chars.find { _1.code_hex.to_i(16) == cp }
        bits  = char ? char.bitmap_bits : "1" * (@bytes * 8) # undef: FF
        line  = Util.bin_to_hex_bytes(bits).join(", ")
        "  #{line}, // 0x%02X" % cp
      end.join("\n")

      <<~C
        /* ---------- ASCII #{@font.name} (#{@bytes} bytes/char) ---------- */
        #include <stdint.h>
        const uint8_t terminus_#{@font.name}[#{RANGE.size * @bytes}] = {
        #{body}
        };
      C
    end
  end

  def self.make(f)
    puts "Building #{f[:dst]}"
    font  = BdfFont.new(f[:name], f[:src], f[:w], f[:h])
    table = AsciiTable.new(font)
    File.write(f[:dst], table.to_c)
  end
end