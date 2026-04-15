#!/usr/bin/env ruby

# ------------------------------------------------------------
# Utility to convert Shinonome BDF fonts into C arrays
#   - ASCII   : Stores characters 0x20–0x7E as a flat 1D array
#   - JISX208 : Organized by ku-ten (row-cell) into 2D arrays with a pointer table
# ------------------------------------------------------------

require "pathname"
require_relative "../../picoruby-bdffont/utils/bdffont_font_convert.rb"

module FontConvert
  Util    = BDFFontConvert::Util
  BdfFont = BDFFontConvert::BdfFont

  class AsciiTable
    RANGE = (0x20..0x7E).freeze

    def initialize(font)
      @font  = font
      @bytes = BDFFontConvert::Util.bytes_per_char(font.w, font.h)
    end

    def to_c
      body = RANGE.map do |cp|
        char = @font.chars.find { _1.code_hex.to_i(16) == cp }
        bits = char ? char.bitmap_bits : "1" * (@bytes * 8)
        line = BDFFontConvert::Util.bin_to_hex_bytes(bits).join(", ")
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
      @bytes = Util.bytes_per_char(font.w, font.h)
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

