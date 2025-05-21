require 'yaml'

class Rapicco
  PALETTE = {
    'w' => 231,  # white
    'p' => 217,  # pink
    'b' => 232,  # black
    'g' => 34,   # shell green
    'y' => 178,  # dark yellow
    'd' => 22,   # dark green
  }
  USA = %w[
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
  ]
  KAME = %w[
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
  ]

  def initialize(path)
    yaml = ""
    @positions = []
    if File.file?(path)
      @file = File.open(path, 'r')
      pos = 0
      @file.each_line do |line|
        break if line.start_with?('# ')
        pos = @file.tell
        yaml << line
      end
      @positions << pos
      @file.each_line do |line|
        @positions << pos if line.start_with?('# ')
        pos = @file.tell
      end
      @positions << pos
    else
      raise "File not found: #{path}"
    end
    config = YAML.load(yaml)
    @usa = Rapicco::Sprite.new(USA, PALETTE)
    @kame = Rapicco::Sprite.new(KAME, PALETTE)
    @parser = Rapicco::Parser.new
    @slide = Rapicco::Slide.new
    @current_page = -1
    @duration = config["duration"] || 60*30
    @interval = @duration.to_f / @slide.page_w
  rescue => e
    puts e.message
    raise e
  end

  def render_page(page)
    return if page == @current_page
    @file.seek(@positions[page])
    page_data = @file.read(@positions[page + 1] - @positions[page])
    page_data.each_line { |line| @parser.parse(line) }
    @slide.render_slide(@parser.dump)
    @current_page = page
    @usa.pos = page * 100 / (@positions.size - 2)
    render_usakame
  end

  def prev_page
    page = [0, @current_page - 1].max
    render_page(page)
  end

  def next_page
    page = [@positions.size - 2, @current_page + 1].min
    render_page(page)
  end

  def render_usakame
    @slide.render_sprite(@usa)
    @slide.render_sprite(@kame)
  end

  def run
    print "\e[?1049h" # DECSET 1049
    _run
    print "\e[?1049l" # DECRST 1049
  end

  def _run
    next_page
    start = now = Time.now.to_i
    while true
      case c = STDIN.read_nonblock(1)&.ord
      when 3 # Ctrl-C
        raise "Interrupted"
      when 104 # h: prev
        prev_page
      when 108 # l: next
        next_page
      end
      if now + @interval < Time.now.to_i
        new_kame_pos = (Time.now.to_i - start) * 100 / @duration
        if @kame.pos != new_kame_pos
          @kame.pos = new_kame_pos
          now += @interval
          render_usakame
        end
      end
    end
  rescue => e
    puts e.message
  ensure
    @file.close
    print "\e[?25h"
  end
end
