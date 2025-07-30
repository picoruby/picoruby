class Rapicco
  class Slide
    COLORS = {
      red:     "\e[48;5;1m",
      green:   "\e[48;5;2m",
      yellow:  "\e[48;5;3m",
      blue:    "\e[48;5;4m",
      magenta: "\e[48;5;5m",
      cyan:    "\e[48;5;6m",
      white:   "\e[48;5;7m",
    }

    def initialize(usakame_h: 12, colors: nil)
      @colors = colors || COLORS
      print "\e[999B\e[999C" # down * 999 and right * 999
      @page_h, @page_w = IO.get_cursor_position
      @page_h -= usakame_h
      print "\e[2J" # clear screen
      print "\e[?25l" # hide cursor
      @line_margin = 3
    end

    attr_accessor :line_margin, :bullet
    attr_reader :page_w

    def check_height
      (@current_page_h -= 1) < 1
    end

    def render_slide(lines)
      @current_page_h = @page_h
      print "\e[1;1H" # home
      lines.each do |line|
        if line[:skip]
          line[:skip].times do
            print "\e[2K\e[E"
            check_height and return
          end
          next
        end
        text_width = 0
        div_count = line[:text].size
        line[:text].each_with_index do |text, div_index|
          Shinonome.draw(line[:font], text[:div], line[:scale]||1) do |height, div_width, widths, glyphs|
            if div_index == 0
              print "\e[2K\e[1E" * (height + @line_margin) + "\e[#{height}A" # clear the line
              if line[:bullet]
                @bullet.show
                text_width = @bullet.width
                print "\e[#{@bullet.height+1}A\e[#{text_width}C"
              end
            end
            text_width += div_width
            color = @colors[text[:color] || :white]
            height.times do |l|
              widths.each_with_index do |width, g|
                scan_line = glyphs[g][l]
                (width - 1).downto(0) do |shift|
                  print((scan_line>>shift)&1 == 1 ? "#{color}  " : "\e[0m\e[2C")
                end
              end
              if div_index == 0
                check_height and return
              end
              # move to the next scan_line's beginning
              print "\e[0m\e[B\e[#{div_width * 2}D" # down * 1 and left * div_width
            end
            if div_index < div_count - 1
              # move to the continue position
              print "\e[#{height}A\e[#{div_width * 2 + 1}C"
            else
              # last text element
              padding = case line[:align]
              when :center
                @page_w / 2 - text_width
              when :right
                @page_w - text_width * 2
              else
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
      x = [@page_w - 18, @page_w * sprite.pos / 100].min
      print "\e[#{@page_h + 1};#{x}H"
      sprite.show
    end
  end
end
