if RUBY_ENGINE == "mruby/c"
  require 'eval'
end

class Rapicco
  class Slide
    COLORS = {
      reset:   "\e[0m",
      red:     "\e[31m",
      green:   "\e[32m",
      yellow:  "\e[33m",
      blue:    "\e[34m",
      magenta: "\e[35m",
      cyan:    "\e[36m",
      white:   "\e[37m",
    }

    # cols and rows are used by CRubyGem Rapicco
    def initialize(usakame_h: 12, colors: nil, cols: nil, rows: nil)
      @usakame_h = usakame_h
      @colors = colors || COLORS
      if cols && rows
        @page_w = cols
        @page_h = rows - usakame_h
      else
        get_screen_size
      end
      @line_margin = 3
      @code_indent = 4
    end

    attr_accessor :line_margin, :bullet
    attr_reader :page_w, :page_h
    attr_accessor :code_indent

    def get_screen_size
      print "\e[999B\e[999C" # down * 999 and right * 999
      @page_h, @page_w = IO.get_cursor_position
      @page_h -= @usakame_h
    end

    def render_slide(lines)
      @current_page_h = @page_h
      print "\e[1;1H" # home
      in_code = false
      line_idx = 0
      while line_idx < lines.size
        line = lines[line_idx]
        line_idx += 1
        next if line[:note]
        total_width_remaining = @page_w
        if line[:skip]
          i = 0
          while i < line[:skip]
            print "\e[2K\e[E"
            check_height and return
            i += 1
          end
          next
        end
        if code_block = line[:code] || line[:eval]
          unless in_code
            i = 0
            while i < @line_margin
              print "\e[2K\e[1E"
              check_height and return
              i += 1
            end
            in_code = true
            print "\e[0m"
          end
          cb_i = 0
          while cb_i < code_block.size
            print (" " * @code_indent), code_block[cb_i], "\e[0K\e[E"
            check_height and return
            cb_i += 1
          end
          if eval_code = line[:eval]&.join("\n")
            eval eval_code
          end
          next
        end
        div_count = line[:text].size
        div_index = 0
        while div_index < div_count
          text = line[:text][div_index]
          font, fontname = line[:font].to_s.split('_')
          case font.downcase
          when "shinonome"
            begin
              font_class = Shinonome
            rescue NameError
              raise "Shinonome font is not available"
            end
          when "terminus"
            font_class = Terminus
          when "karmatic-arcade"
            font_class = KarmaticArcade
          else
            raise "Unknown font: #{line[:font]}"
          end
          result = font_class&.draw(fontname, text[:div], line[:scale]||1)
          if result
            height = result[0]
            div_width = result[1]
            widths = result[2]
            glyphs = result[3]
            height /= 2
            if div_index == 0
              print "\e[2K\e[1E" * (height + @line_margin) + "\e[#{height}A" # clear the line
              if line[:bullet]
                @bullet.show
                print "\e[#{@bullet.height+1}A\e[#{@bullet.width}C"
                total_width_remaining -= @bullet.width
              end
            end
            print @colors[text[:color] || :white]
            overflow = false
            l = 0
            while l < height
              width_remaining = total_width_remaining
              overflow = false
              width_drew = 0
              g = 0
              while g < widths.size
                width = widths[g]
                scan_line_upper = glyphs[g][l*2]
                scan_line_lower = glyphs[g][l*2+1]
                shift = width - 1
                while 0 <= shift
                  if (scan_line_upper>>shift)&1 == 1
                    if (scan_line_lower>>shift)&1 == 1
                      print "\xE2\x96\x88" # Full Block U+2588
                    else
                      print "\xE2\x96\x80" # Upper Half Block U+2580
                    end
                  else
                    if (scan_line_lower>>shift)&1 == 1
                      print "\xE2\x96\x84" # Lower Half Block U+2584
                    else
                      print " "
                    end
                  end
                  width_drew += 1
                  width_remaining -= 1
                  if width_remaining <= 1 # Just before wrapping
                    overflow = true
                    break
                  end
                  shift -= 1
                end
                break if overflow
                g += 1
              end
              if div_index == 0
                check_height and return
              end
              # move to the next scan_line's beginning
              print "\e[B\e[#{width_drew}D" # down * 1 and left * width_drew
              l += 1
            end
            unless overflow
              total_width_remaining -= div_width
              if div_index < div_count - 1
                # move to the continue position
                print "\e[#{height}A\e[#{div_width + 1}C"
              else
                # last text element
                padding = if line[:bullet]
                            0
                          else
                            case line[:align]
                            when :center
                              total_width_remaining / 2
                            when :right
                              total_width_remaining
                            else # :left
                              0
                            end
                          end
                if 0 < padding
                  print "\e[F\e[#{padding}@" * height + "\e[#{height}B"
                end
              end
            end
          end
          div_index += 1
        end
      end
      print "\e[2K\e[E" * @current_page_h
      GC.start if RUBY_ENGINE == "mruby"
    end

    def render_sprite(sprite)
      x = [@page_w - sprite.width, @page_w * sprite.pos / 100].min
      print "\e[#{@page_h + 1};#{x}H"
      sprite.show
    end

    private

    def check_height
      (@current_page_h -= 1) < 1
    end

  end
end
