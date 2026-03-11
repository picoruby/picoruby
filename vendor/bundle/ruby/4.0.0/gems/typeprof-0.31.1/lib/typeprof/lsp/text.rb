module TypeProf::LSP
  class Text
    #: (String, String, Integer) -> void
    def initialize(path, text, version)
      @path = path
      @lines = Text.split(text)
      @version = version
    end

    attr_reader :path, :lines, :version

    def self.split(str)
      lines = str.lines
      lines << "" if lines.empty? || lines.last.include?("\n")
      lines
    end

    def string
      @lines.join
    end

    def apply_changes(changes, version)
      changes.each do |change|
        change => {
          range: {
              start: { line: start_row, character: start_col },
              end:   { line: end_row  , character: end_col   }
          },
          text: new_text,
        }

        new_text = Text.split(new_text)

        prefix = @lines[start_row][0...start_col]
        suffix = @lines[end_row][end_col...]
        if new_text.size == 1
          new_text[0] = prefix + new_text[0] + suffix
        else
          new_text[0] = prefix + new_text[0]
          new_text[-1] = new_text[-1] + suffix
        end
        @lines[start_row .. end_row] = new_text
      end

      validate

      @version = version
    end

    def validate
      raise unless @lines[0..-2].all? {|s| s.count("\n") == 1 && s.end_with?("\n") }
      raise unless @lines[-1].count("\n") == 0
    end

    def modify_for_completion(changes, pos)
      pos => { line: row, character: col }
      if col >= 2 && @lines[row][col - 1] == "." && (col == 1 || @lines[row][col - 2] != ".")
        @lines[row][col - 1] = " "
        yield string, ".", { line: row, character: col - 2}
        @lines[row][col - 1] = "."
      elsif col >= 3 && @lines[row][col - 2, 2] == "::"
        @lines[row][col - 2, 2] = "  "
        yield string, "::", { line: row, character: col - 3 }
        @lines[row][col - 2, 2] = "::"
      else
        yield string, nil, { line: row, character: col }
      end
    end
  end
end
