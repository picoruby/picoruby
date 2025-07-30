class Rapicco
  class Sprite
    DATA = {
      "hasumikin" => {
        rapiko: %w[
          ;......www..ww
          ;......wpw..wp
          ;....wwwpwwwwpw
          ;...wwwwwwwwwwww
          ;...wwwwwbwwwwbw
          ;....wwwwwwwbwww
          ;.....wwwwwwwww
          ;.....pppppppppp
          ;....ppwwppppppww
          ;...wpppppppppp
          ;.....www....www
        ],
        camerlengo: %w[
          .
          .
          .
          .
          .............yyyyy
          ............yyyddyy
          ....ggggggggyyyyyyy
          ...ggggggggggyyyyy
          ..gggggggggggg
          yygggggggggggg
          ...yyy......yyy
        ],
        bullet: %w[
          .
          .
          ......rrrrrr
          ....rrrrrrrrrr
          ..rrrrrrrrrrrrrr.....
          ....rrrrrrrrrr
          ......rrrrrr
          ........rr
        ],
        palette: {
          'w' => 231,  # white
          'p' => 217,  # pink
          'b' => 232,  # black
          'g' => 34,   # shell green
          'y' => 178,  # dark yellow
          'd' => 22,   # dark green
          'r' => 196,  # red
        }
      },
      "picochobishiba" => {
        rapiko: %w[
          ;.w.w
          ;.w.w
          ;.wwvw
          ;.www
          ;wwlwl
        ],
        camerlengo: %w[
          .
          ...yd
          .ggyy
          gggg
          y..y
        ],
        bullet: %w[
          .
          ...rrr
          ..rrrrr..
          ...rrr
          ....r
        ],
       palette: {
          'w' => 231,
          'g' => 34,
          'y' => 178,
          'd' => 22,
          'r' => 196,
          'l' => 252,
          'v' => 124,
        }
      },
      "" => {
        rapiko: %w[
        ],
        camerlengo: %w[
        ],
        bullet: %w[
        ],
        palette: {
        }
      },
    }
    def initialize(name, author)
      rows = DATA[author][name]
      palette = DATA[author][:palette]
      @pos = 0
      @data = rows.map do |row|
        out = ""
        i = 0
        prev = nil
        adjust = 0
        while i < row.length
          ch = row[i]
          case ch
          when ';'
            adjust = -1
            out << "\e[2K"
            i += 1
          when '.'
            j = i
            j += 1 while j < row.length && row[j] == '.'
            n = j - i
            out << "\e[0m" if prev
            prev = nil
            out << (n == 1 ? "\e[C" : "\e[#{n}C")
            i = j
          when nil
            raise "Unexpected nil character in row"
          else
            color = palette[ch]
            if color != prev
              out << "\e[48;5;#{color}m"
              prev = color
            end
            out << ' '
            i += 1
          end
        end
        out << "\e[0m" if prev
        out << "\e[#{row.length + adjust}D\e[B" # Carriage return
        out
      end
      @width = rows.map(&:length).max
      @height = @data.size
    end

    attr_accessor :pos
    attr_reader :width, :height

    def show
      @data.each { |l| print l }
    end
  end
end
