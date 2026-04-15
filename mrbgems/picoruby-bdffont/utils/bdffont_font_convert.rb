#!/usr/bin/env ruby

# Shared BDF font conversion utilities.
# BdfFont + AsciiTableGenerator    : fixed-width fonts (Terminus, Shinonome)
# PropBdfFont + PropAsciiTableGenerator : proportional fonts (KarmaticArcade)

require "pathname"

module BDFFontConvert
  module Util
    def self.bytes_per_char(w, h)
      ((w * h) + 7) / 8
    end

    # "01010101..." -> ["0x55", "0xAA", ...]
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
      i     = 0
      chars = []

      while i < lines.size
        unless lines[i].start_with?("STARTCHAR")
          i += 1
          next
        end

        enc_line = lines[i + 1..].find { _1.start_with?("ENCODING") }
        code_dec = enc_line.split[1].to_i
        code_hex = "%04X" % code_dec

        bitmap_start = lines[i..].index { _1 == "BITMAP" }
        raise "BITMAP not found for #{code_hex}" unless bitmap_start
        bitmap_start += i + 1
        bitmap_end   = bitmap_start + @h - 1

        bitmap_rows = []
        (bitmap_start..bitmap_end).each do |ln|
          row_hex  = lines[ln].strip
          need_hex = (@w + 3) / 4
          row_hex  = row_hex.ljust(need_hex, "0")[0, need_hex]
          row_bits = row_hex.to_i(16).to_s(2).rjust(need_hex * 4, "0")[0, @w]
          bitmap_rows << row_bits
        end

        chars << Char.new(code_hex, bitmap_rows.join)

        endchar = lines[i..].index("ENDCHAR")
        i += endchar + 1
      end
      chars
    end
  end

  class AsciiTableGenerator
    RANGE = (0x20..0x7E).freeze

    # array_name: C identifier for the generated array (e.g. "terminus_6x12")
    def initialize(font, array_name)
      @font       = font
      @array_name = array_name
      @bytes      = Util.bytes_per_char(font.w, font.h)
    end

    def to_c
      body = RANGE.map do |cp|
        char = @font.chars.find { _1.code_hex.to_i(16) == cp }
        bits = char ? char.bitmap_bits : "1" * (@bytes * 8)
        line = Util.bin_to_hex_bytes(bits).join(", ")
        "  #{line}, // 0x%02X" % cp
      end.join("\n")

      <<~C
        /* ---------- ASCII #{@font.name} (#{@bytes} bytes/char) ---------- */
        #include <stdint.h>
        const uint8_t #{@array_name}[#{RANGE.size * @bytes}] = {
        #{body}
        };
      C
    end
  end

  # Proportional (variable-width) BDF font parser.
  # Reads height from FONTBOUNDINGBOX and ascent from FONT_ASCENT automatically.
  class PropBdfFont
    Char = Struct.new(:code, :dwidth, :bbx_w, :bbx_h, :bbx_x, :bbx_y, :bitmap_rows)

    attr_reader :chars, :height, :ascent

    def initialize(path)
      @path   = Pathname(path)
      @height = nil
      @ascent = nil
      @chars  = parse_bdf
    end

    private

    def parse_bdf
      chars     = {}
      current   = nil
      in_bitmap = false
      bm_rows   = []

      @path.readlines(chomp: true).each do |ln|
        case ln
        when /\AFONT_ASCENT\s+(\d+)/
          @ascent = $1.to_i
        when /\AFONTBOUNDINGBOX\s+\d+\s+(\d+)/
          @height = $1.to_i
        when /\ASTARTCHAR/
          current   = {}
          in_bitmap = false
          bm_rows   = []
        when /\AENCODING\s+(-?\d+)/
          current[:code] = $1.to_i
        when /\ADWIDTH\s+(\d+)/
          current[:dwidth] = $1.to_i
        when /\ABBX\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(-?\d+)/
          current[:bbx_w] = $1.to_i
          current[:bbx_h] = $2.to_i
          current[:bbx_x] = $3.to_i
          current[:bbx_y] = $4.to_i
        when "BITMAP"
          in_bitmap = true
        when "ENDCHAR"
          if in_bitmap && current[:code]
            current[:bitmap_rows] = bm_rows.dup
            c = Char.new(
              current[:code], current[:dwidth],
              current[:bbx_w], current[:bbx_h],
              current[:bbx_x], current[:bbx_y],
              current[:bitmap_rows]
            )
            chars[c.code] = c
          end
          in_bitmap = false
          bm_rows   = []
        else
          bm_rows << ln.strip if in_bitmap
        end
      end
      chars
    end
  end

  # Generates C tables for a proportional (variable-width) ASCII font.
  # Output: {name}_widths[], {name}_offsets[], {name}_bitmaps[]
  class PropAsciiTableGenerator
    RANGE = (0x20..0x7E).freeze

    def initialize(font, table_name)
      @font       = font
      @table_name = table_name
      @height     = font.height
      @ascent     = font.ascent
    end

    def to_c
      cells  = RANGE.map { |cp| glyph_cell(cp) }
      widths = cells.map { _1[:cell_w] }

      offsets = []
      offset  = 0
      cells.each do |cell|
        offsets << offset
        offset  += cell[:bytes].size
      end
      total_bytes = offset

      widths_body = widths.each_with_index.map { |w, i|
        "  #{w}, // 0x%02X" % (RANGE.first + i)
      }.join("\n")

      offsets_body = offsets.each_with_index.map { |o, i|
        "  #{o}, // 0x%02X" % (RANGE.first + i)
      }.join("\n")

      bitmaps_body = cells.each_with_index.map { |cell, i|
        cp  = RANGE.first + i
        hex = cell[:bytes].map { "0x%02X" % _1 }.join(", ")
        "  // 0x%02X (w=%d)\n  %s," % [cp, cell[:cell_w], hex]
      }.join("\n")

      prefix = @table_name.upcase

      <<~C
        /* #{@table_name} #{@height}px font table (auto-generated) */
        #include <stdint.h>

        #define #{prefix}_HEIGHT      #{@height}
        #define #{prefix}_ASCII_BASE  0x20
        #define #{prefix}_ASCII_COUNT #{RANGE.size}

        const uint8_t #{@table_name}_widths[#{RANGE.size}] = {
        #{widths_body}
        };

        const uint16_t #{@table_name}_offsets[#{RANGE.size}] = {
        #{offsets_body}
        };

        const uint8_t #{@table_name}_bitmaps[#{total_bytes}] = {
        #{bitmaps_body}
        };
      C
    end

    private

    def glyph_cell(cp)
      char = @font.chars[cp]
      unless char
        return { cell_w: @height, bytes: [0] * ((@height * @height + 7) / 8) }
      end

      cell_w = [char.dwidth, char.bbx_x + char.bbx_w].max
      cell_w = 1 if cell_w < 1

      grid = Array.new(@height) { Array.new(cell_w, 0) }

      if char.bbx_h > 0 && char.bbx_w > 0
        cell_row_start = @ascent - (char.bbx_y + char.bbx_h)
        cell_col_start = char.bbx_x

        char.bitmap_rows.each_with_index do |row_hex, r|
          cell_row = cell_row_start + r
          next if cell_row < 0 || cell_row >= @height

          bytes_needed = (char.bbx_w + 7) / 8
          padded    = row_hex.ljust(bytes_needed * 2, "0")[0, bytes_needed * 2]
          row_bytes = padded.scan(/.{2}/).map { _1.to_i(16) }

          char.bbx_w.times do |bx|
            cell_col = cell_col_start + bx
            next if cell_col < 0 || cell_col >= cell_w

            byte_idx = bx / 8
            bit_pos  = 7 - (bx % 8)
            bit      = (row_bytes[byte_idx] >> bit_pos) & 1
            grid[cell_row][cell_col] = bit
          end
        end
      end

      { cell_w: cell_w, bytes: grid_to_bytes(grid, cell_w) }
    end

    def grid_to_bytes(grid, cell_w)
      bits = grid.flat_map { |row| row[0, cell_w] }
      pad  = (-bits.size) % 8
      bits += [0] * pad unless pad.zero?
      bits.each_slice(8).map { |b| b.inject(0) { |acc, bit| (acc << 1) | bit } }
    end
  end
end
