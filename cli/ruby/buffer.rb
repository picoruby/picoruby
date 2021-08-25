class Buffer
  def initialize
    @cursor = {x: 0, y: 0}
    clear
  end
  attr_reader :lines
  def cursor
    # Human readable
    {x: @cursor[:x] + 1, y: @cursor[:y] + 1}
  end
  def clear
    @lines = [""]
    home
  end
  def dump
    @lines.join("\n")
  end
  def home
    @cursor[:x] = 0
    @cursor[:y] = 0
  end
  def head
    @cursor[:x] = 0
  end
  def tail
    @cursor[:x] = @lines[@cursor[:y]].length
  end
  def left
    if @cursor[:x] > 0
      @cursor[:x] -= 1
    else
      if @cursor[:y] > 0
        up
        tail
      end
    end
  end
  def right
    if @lines[@cursor[:y]].length > @cursor[:x]
      @cursor[:x] += 1
    else
      if @lines.length > @cursor[:y]
        down
        head
      end
    end
  end
  def up
    @cursor[:y] -= 1 if @cursor[:y] > 0
    if @cursor[:x] > @lines[@cursor[:y]].length
      @cursor[:x] = @lines[@cursor[:y]].length
    end
  end
  def down
    @cursor[:y] += 1 if @lines.length > @cursor[:y] + 1
  end
  def put(c)
    line = @lines[@cursor[:y]]
    if c.is_a?(String)
      line = line[0, @cursor[:x]].to_s + c + line[@cursor[:x], 65535].to_s
      @lines[@cursor[:y]] = line
      right
    else
      case c
      when :ENTER
        new_line = line[@cursor[:x], 65535]
        @lines[@cursor[:y]] = line[0, @cursor[:x]].to_s
        @lines.my_insert(@cursor[:y] + 1, new_line)
        down
        head
      when :BSPACE
        if @cursor[:x] > 0
          line = line[0, @cursor[:x] - 1].to_s + line[@cursor[:x], 65535].to_s
          @lines[@cursor[:y]] = line
          left
        else
          if @cursor[:y] > 0
            @cursor[:x] = @lines[@cursor[:y] - 1].length
            @lines[@cursor[:y] - 1] += @lines[@cursor[:y]]
            @lines.delete_at @cursor[:y]
            up
          end
        end
      when :DOWN
        down
      when :UP
        up
      when :RIGHT
        right
      when :LEFT
        left
      when :HOME
        home
      end
    end
  end
  def current_tail(n = 1)
    @lines[@cursor[:y]][@cursor[:x] - n, 65535].to_s
  end
end

$buffer_lock = true
