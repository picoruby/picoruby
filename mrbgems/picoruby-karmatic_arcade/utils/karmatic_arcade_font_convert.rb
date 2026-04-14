#!/usr/bin/env ruby

# Utility to convert KarmaticArcade BDF font into C arrays.
# Produces variable-width glyph table for ASCII 0x20-0x7E.

require "pathname"

module KarmaticArcadeFontConvert
  FONT_H      = 20
  FONT_ASCENT = 16
  ASCII_RANGE = (0x20..0x7E).freeze

  module Util
    # Convert a 2D bit grid (rows of 0/1 ints, cell_w wide) into
    # packed bytes (MSB-first, row by row, padded to byte boundary).
    def self.grid_to_bytes(grid, cell_w)
      bits = grid.flat_map { |row| row[0, cell_w] }
      pad  = (-bits.size) % 8
      bits += [0] * pad unless pad.zero?
      bits.each_slice(8).map { |b| b.inject(0) { |acc, bit| (acc << 1) | bit } }
    end
  end

  class BdfChar
    attr_reader :code, :dwidth, :bbx_w, :bbx_h, :bbx_x, :bbx_y, :bitmap_rows

    def initialize(attrs)
      @code        = attrs[:code]
      @dwidth      = attrs[:dwidth]
      @bbx_w       = attrs[:bbx_w]
      @bbx_h       = attrs[:bbx_h]
      @bbx_x       = attrs[:bbx_x]
      @bbx_y       = attrs[:bbx_y]
      @bitmap_rows = attrs[:bitmap_rows]
    end
  end

  class BdfFont
    def initialize(path)
      @path  = Pathname(path)
      @chars = parse_bdf
    end
    attr_reader :chars

    private

    def parse_bdf
      chars     = {}
      current   = nil
      in_bitmap = false
      bm_rows   = []

      @path.readlines(chomp: true).each do |ln|
        case ln
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
            c = BdfChar.new(current)
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

  class AsciiTable
    RANGE = ASCII_RANGE

    def initialize(font)
      @font = font
    end

    # Build glyph cell for one code point.
    # Returns { cell_w: Integer, bytes: Array<Integer> }
    def glyph_cell(cp)
      char = @font.chars[cp]
      unless char
        return { cell_w: 10, bytes: [0] * ((10 * FONT_H + 7) / 8) }
      end

      # cell_w: wide enough to contain the full glyph and advance the cursor
      cell_w = [char.dwidth, char.bbx_x + char.bbx_w].max
      cell_w = 1 if cell_w < 1

      grid = Array.new(FONT_H) { Array.new(cell_w, 0) }

      if char.bbx_h > 0 && char.bbx_w > 0
        # row in cell where the top of the BBX sits
        cell_row_start = FONT_ASCENT - (char.bbx_y + char.bbx_h)
        cell_col_start = char.bbx_x

        char.bitmap_rows.each_with_index do |row_hex, r|
          cell_row = cell_row_start + r
          next if cell_row < 0 || cell_row >= FONT_H

          bytes_needed = (char.bbx_w + 7) / 8
          padded    = row_hex.ljust(bytes_needed * 2, "0")[0, bytes_needed * 2]
          row_bytes = padded.scan(/.{2}/).map { _1.to_i(16) }

          char.bbx_w.times do |bx|
            cell_col = cell_col_start + bx
            next if cell_col < 0 || cell_col >= cell_w

            byte_idx = bx / 8
            bit_pos  = 7 - (bx % 8)
            bit = (row_bytes[byte_idx] >> bit_pos) & 1
            grid[cell_row][cell_col] = bit
          end
        end
      end

      { cell_w: cell_w, bytes: Util.grid_to_bytes(grid, cell_w) }
    end

    def to_c
      cells   = RANGE.map { |cp| glyph_cell(cp) }
      widths  = cells.map { _1[:cell_w] }
      offsets = []
      offset  = 0
      cells.each do |cell|
        offsets << offset
        offset  += cell[:bytes].size
      end
      total_bytes = offset

      widths_body = widths.each_with_index.map { |w, i|
        cp = RANGE.first + i
        "  #{w}, // 0x%02X" % cp
      }.join("\n")

      offsets_body = offsets.each_with_index.map { |o, i|
        cp = RANGE.first + i
        "  #{o}, // 0x%02X" % cp
      }.join("\n")

      bitmaps_body = cells.each_with_index.map { |cell, i|
        cp   = RANGE.first + i
        hex  = cell[:bytes].map { "0x%02X" % _1 }.join(", ")
        "  // 0x%02X (w=%d)\n  %s," % [cp, cell[:cell_w], hex]
      }.join("\n")

      <<~C
        /* KarmaticArcade 20px font table (auto-generated) */
        #include <stdint.h>

        #define KARMATIC_ARCADE_HEIGHT      20
        #define KARMATIC_ARCADE_ASCII_BASE  0x20
        #define KARMATIC_ARCADE_ASCII_COUNT #{RANGE.size}

        const uint8_t karmatic_arcade_widths[#{RANGE.size}] = {
        #{widths_body}
        };

        const uint16_t karmatic_arcade_offsets[#{RANGE.size}] = {
        #{offsets_body}
        };

        const uint8_t karmatic_arcade_bitmaps[#{total_bytes}] = {
        #{bitmaps_body}
        };
      C
    end
  end

  def self.make(dst_path, bdf_path)
    puts "Building #{dst_path}"
    font  = BdfFont.new(bdf_path)
    table = AsciiTable.new(font)
    File.write(dst_path, table.to_c)
  end
end
