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

    def initialize(usakame_h: 12, colors: nil)
      @usakame_h = usakame_h
      @colors = colors || COLORS
      get_screen_size
      @line_margin = 2
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
      lines.each do |line|
        next if line[:note]
        total_width_remaining = @page_w
        if line[:skip]
          line[:skip].times do
            print "\e[2K\e[E"
            check_height and return
          end
          next
        end
        if code_block = line[:code] || line[:eval]
          unless in_code
            @line_margin.times do
              print "\e[2K\e[1E"
              check_height and return
            end
            in_code = true
            print "\e[0m"
          end
          code_block.each do |code_line|
            print (" " * @code_indent), code_line, "\e[0K\e[E"
            check_height and return
          end
          if eval_code = line[:eval]&.join("\n")
            eval eval_code
          end
          next
        end
        div_count = line[:text].size
        line[:text].each_with_index do |text, div_index|
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
          else
            raise "Unknown font: #{line[:font]}"
          end
          font_class&.draw(fontname, text[:div], line[:scale]||1) do |height, div_width, widths, glyphs|
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
            (height).times do |l|
              width_remaining = total_width_remaining
              overflow = false
              width_drew = 0
              widths.each_with_index do |width, g|
                scan_line_upper = glyphs[g][l*2]
                scan_line_lower = glyphs[g][l*2+1]
                (width - 1).downto(0) do |shift|
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
                    break 0
                  end
                end
                break [0] if overflow
              end
              if div_index == 0
                check_height and return
              end
              # move to the next scan_line's beginning
              print "\e[B\e[#{width_drew}D" # down * 1 and left * width_drew
            end
            next if overflow
            total_width_remaining -= div_width
            if div_index < div_count - 1
              # move to the continue position
              print "\e[#{height}A\e[#{div_width + 1}C"
            else
              # last text element
              padding = case line[:align]
              when :center
                total_width_remaining / 2
              when :right
                total_width_remaining
              else # :left
                0
              end
              if 0 < padding
                print "\e[F\e[#{padding}@" * height + "\e[#{height}B"
              end
            end
          end
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
