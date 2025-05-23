class Rapicco
  class Sprite
    def initialize(rows, palette)
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
