class Rapicco
  class Sprite
    PALETTE = {
      "hasumikin" => {
        'w' => 231,  # white
        'p' => 217,  # pink
        'b' => 232,  # black
        'g' => 34,   # shell green
        'y' => 178,  # dark yellow
        'd' => 22,   # dark green
        'r' => 196   # red
      },
      "picochobishiba" => {
        'w' => 231,
        'g' => 34,
        'y' => 178,
        'd' => 22,
        'r' => 196,
        'l' => 252,
        'v' => 124
      },
      "chobishiba" => {
        'w' => 231,
        'p' => 217,
        'b' => 248,
        'g' => 34,
        'y' => 178,
        'd' => 22,
        'r' => 196,
        'v' => 124,
        'k' => 0,
      },
      "ydah" => {
        'w' => 231,
        'k' => 236,
        'c' => 131,
        'p' => 217,
        'b' => 232,
        'g' => 34,
        'y' => 226,
        'd' => 22,
        'r' => 196,
      },
      "youchan" => {
        'w' => 231,
        'p' => 217,
        's' => 160,
        'l' => 224,
        'b' => 232,
        'g' => 34,
        'o' => 239,
        'y' => 220,
        'd' => 22,
        'r' => 196,
      },
      "ogom" => {
        'w' => 231,
        'p' => 217,
        'b' => 232,
        'g' => 34,
        'y' => 11,
        'd' => 22,
        'r' => 196,
        's' => 255,
        'l' => 42,
        'k' => 3,
      }
    }
    DATA = {
      "hasumikin" => {
        rapiko: %w[
          ......www..ww
          ......wpw..wp
          ....wwwpwwwwpw
          ...wwwwwwwwwwww
          ...wwwwwbwwwwbw
          ....wwwwwwwbwww
          .....wwwwwwwww
          .....pppppppppp
          ....ppwwppppppww
          ...wpppppppppp
          .....www....www
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
          ..rrrrrrrrrrrrrr....
          ....rrrrrrrrrr
          ......rrrrrr
          ........rr
        ]
      },
      "picochobishiba" => { # https://github.com/picoruby/usakame/pull/3
        rapiko: %w[
          .w.w
          .w.w
          .wwvw
          .www
          wwlwl
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
        ]
      },
      "chobishiba" => { # https://github.com/picoruby/usakame/pull/1
        rapiko: %w[
          .......wwwwww
          .........wwwwww
          ...........wwwwww
          ...wwwwwwwwwwwwwwww
          ...wwppppppwwwwvvwwww
          ...........wwwwwwwwwwpp
          ...........wwwwwwwwww
          .......wwwwwwwwwwww
          .....wwwwwwwwwwwwww
          ...wwwwwwbbbbwwwwww
          ...wwwwwwwwwwbbwwwwww
          .......wwwwwwwwbbwwwwww
        ],
        camerlengo: %w[
          .
          .
          .
          .
          ........gggddddggg
          ......gddddggggddddg..yyyy
          ....dddggggddddggggdddyykkyy
          ....gggddddggggddddgggyyyyyy
          yy..dddggggddddggggdddyyyy
          ..yyddddddddddddddddddyy
          ....yyyy..........yyyy
          ....yy..............yy
        ],
        bullet: %w[
          ....vvvvwwrrrrrrvvvvvv
          ..vvvvwwrrrrrrrrrrvvvvvv
          vvvvvvvvrrrrrrrrrrvvvvvvvv....
          vvwwwwrrwwrrwwwwwwrrwwwwvv....
          ..vvvvrrrrrrrrrrrrrrvvvv
          ....vvvvrrrrrrrrrrvvvv
          ......vvrrrrrrrrrrvv
          ........vvrrrrrrvv
          ..........rrrrrr
          ............rr
        ]
      },
      "ydah" => { # https://github.com/picoruby/usakame/pull/2
        rapiko: %w[
          ...............kkkkkkkkkk
          ...............kkwwwwkkwwkk
          .......kkkkkk..kkwwwwkkwwkk
          .....kkwwwwrrkkwwwwwwkkwwwwkk
          ...kkrrrrrrrrrrkkwwwwwwwwwwwwkk
          .....kkrrrrrrkkkkwwwwwwwwwwwwkk
          .......kkrrcckkwwwwwwwwwwkkwwwwkk
          .........kkkkccwwwwwwrrwwwwwwwwkk
          .......kkwwkkwwccwwwwwwwwwwwwkk
          .....kkwwwwkkwwwwwwwwwwwwkkwwkk
          .......kkwwkkwwwwwwwwwwkkkkwwkk
          .........kkkkkkwwwwwwwwwwwwwwkk
          ...............kkkkwwwwwwkkkk
          ...................kkkkkk
        ],
        camerlengo: %w[
          ....kkkkkkkkkk
          ..kkwwwwrrrrrrkk......kkkkkk
          kkrrrrrrrrrrrrrrkk..kkyyyyyykk
          ..kkrrrrrrrrrrkkkkkkkkyyyyyyyykk
          ....kkrrrrrrkkggggkkkkyyyyyyyyyykk
          ....kkkkrrccggggggkkyyyyyyyyyyyykk
          ....kkggkkggccggkkkkyyyykkkkkkyykk
          ....kkggggkkkkccyyyyyyyyyyrrwwrryykk
          ....kkggkkkkyyyyyykkyyyyyyyyrrrryykk
          ......kkkkkkyyyyyyyyyykkyyyyyyyyyykk
          ....kkyyyyyyyykkyyyyyykkkkyyyyyykkkk
          ....kkyyyyyyyyyykkyyyyyykkkkkkkk
          ......kkyyyyyykkkkkkyyyyyykk
          ........kkkkkkkkkk..kkkkkkkk
        ],
        bullet: %w[
          .
          .
          ........kkkkkk
          ......kkrrrrrrkk
          ....kkrwwwwrrrrrkk
          ..kkrrrrrrrrrrrrrrkk....
          ....kkrrrrrrrrrrkk
          ......kkrrrrrrkk
          ........kkrrkk
          ..........kk
        ]
      },
      "youchan" => { # https://github.com/picoruby/usakame/pull/4
        rapiko: %w[
          ....www..www
          .....wpw..wpw
          .....wpw..wpw
          .....wwwwwwwww
          ....wwwwwbwwbww
          .....wwwwwwwrrrs
          ......wwwwwrlrrrs
          .....pppppwwwrsww
          ...wwpppppppp
          ....www....www
        ],
        camerlengo: %w[
          .
          .....rrrs.....yyyy
          ....rlrrrs...yyydyy
          ......rs.....yyyyyy
          ....gogogog..yyyyy
          ...gooooooogyyyyy
          ..gggogogogggyy
          .gggoooooooggg
          yygggogogogggg
          ...yyy......yyy
        ],
        bullet: %w[
          .
          .
          .....llrrrrrs
          ...llrrrrrrrsss
          ..rllrrrssssssss....
          ...rrrrrrrrssss
          .....rrrrrsss
          .......rrss
          ........rs
        ]
      },
      "ogom" => { # https://github.com/picoruby/usakame/pull/5
        rapiko: %w[
          ..........ssss....ssss
          ..........sswwss..sswwss
          ..........sswwwwsssswwss
          ............sswwsssswwss
          ..............sssswwwwwwss
          ............sswwwwwwwwwwwwss
          ............sswwwwwwwwbbwwss
          ............sswwwwwwbbbbwwss
          ..ssss..sssssssswwppppwwss
          sswwwwsswwwwwwwwsswwwwss
          sswwwwsswwwwwwwwwwwwwwss
          ..sssswwwwwwwwwwwwwwwwss
          ....sswwwwwwwwwwwwwwwwssss
          ..sswwwwwwwwsssswwwwwwwwss
          ....ssssssss....ssssssssss
        ],
        camerlengo: %w[
          .
          .
          .
          .
          .
          .
          ......ggllggll
          ....ggllggllggll..kkyyyyyy
          ..ggllggllggllggllyyyyyyyyyy
          ..llggllggllggllggyywwbbyyyy
          llggllggllggllggllyybbbbyyyy
          ..llggllggllggllyyppppyyyy
          ....yyyyyyyyyyyyyyyykk
          ..yyyyyykk..kkyyyy
          ..yyyykk......yyyyyy
        ],
        bullet: %w[
          .
          .
          .
          ......rrpprrpprr
          ....rrwwrrpprrpprr
          ..rrpprrpprrpprrpprr....
          ....rrpprrpprrpprr
          ......rrpprrpprr
          ........rrpprr
          ..........rr
        ]
      },
    }

    def initialize(name, author)
      rows = DATA[author][name]
      palette = PALETTE[author]
      @pos = 0
      @data = rows.map do |row|
        out = (name == :rapiko) ? "\e[2K" : ""
        i = 0
        prev = nil
        while i < row.length
          ch = row[i]
          case ch
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
        out << "\e[#{row.length}D\e[B" # Carriage return
        out
      end
      @width = rows.map {|row| row.length}.max
      @height = @data.size
    end

    attr_accessor :pos
    attr_reader :width, :height

    def show
      @data.each { |l| print l }
    end
  end
end
