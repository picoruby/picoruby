module TypeProf
  class CodePosition
    def initialize(lineno, column)
      @lineno = lineno
      @column = column
    end

    def self.from_lsp(pos)
      new(pos[:line] + 1, pos[:character])
    end

    def to_lsp
      { line: @lineno - 1, character: @column }
    end

    attr_reader :lineno, :column

    def <=>(other)
      cmp = @lineno <=> other.lineno
      cmp == 0 ? @column <=> other.column : cmp
    end

    include Comparable

    def ==(other)
      @lineno == other.lineno && @column == other.column
    end

    alias eql? ==

    def hash
      [@lineno, @column].hash
    end

    def to_s
      "(%d,%d)" % [@lineno, @column]
    end

    alias inspect to_s

    def left
      raise if @column == 0
      CodePosition.new(@lineno, @column - 1)
    end

    def right
      CodePosition.new(@lineno, @column + 1)
    end
  end

  class CodeRange
    def initialize(first, last)
      @first = first
      @last = last
      raise unless first
    end

    def self.from_node(node, encoding = Encoding::UTF_16LE)
      node = node.location if node.respond_to?(:location)
      if node.is_a?(Prism::Location)
        pos1 = CodePosition.new(node.start_line, node.start_code_units_column(encoding))
        pos2 = CodePosition.new(node.end_line, node.end_code_units_column(encoding))
      elsif node.is_a?(RBS::Location)
        pos1 = CodePosition.new(*node.start_loc)
        pos2 = CodePosition.new(*node.end_loc)
      else
        p node.class.ancestors
        raise "unknown type: #{ node.class }"
      end
      new(pos1, pos2)
    end

    def self.[](a, b, c, d)
      pos1 = CodePosition.new(a, b)
      pos2 = CodePosition.new(c, d)
      new(pos1, pos2)
    end

    def to_lsp
      { start: @first.to_lsp, end: @last.to_lsp }
    end

    attr_reader :first, :last

    def include?(pos)
      @first <= pos && pos < @last
    end

    def ==(other)
      @first == other.first && @last == other.last
    end

    alias eql? ==

    def hash
      [@first, @last].hash
    end

    def to_s
      "%p-%p" % [@first, @last]
    end

    alias inspect to_s

    def ==(other)
      @first == other.first && @last == other.last
    end

    def <=>(other)
      cmp = @first <=> other.first
      cmp == 0 ? @last <=> other.last : cmp
    end
  end
end
