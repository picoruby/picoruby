require 'yaml'

class Rapicco

  def initialize(path)
    yaml = ""
    @positions = []
    unless File.file?(path)
      raise "File not found: #{path}"
    end
    @file = File.open(path, 'r')
    if @file.gets&.chomp == "---" # Front-matter
      @file.each_line do |line|
        break if line.chomp == "---"
        yaml << line
      end
    else
      @file.seek(0)
    end
    pos = @file.tell
    in_code_block = false
    @file.each_line do |line|
      in_code_block = !in_code_block if line.start_with?('```')
      @positions << pos if line.start_with?('# ') && !in_code_block
      pos = @file.tell
    end
    @positions << pos
    if @positions.size < 3
      raise "Slides must be more than 2 pages"
    end
    # @type var config: Hash[String, String]
    config = YAML.load(yaml)
    sprite = config["sprite"] || "hasumikin"
    @rapiko = Rapicco::Sprite.new(:rapiko, sprite)
    @camerlengo = Rapicco::Sprite.new(:camerlengo, sprite)
    @parser = Rapicco::Parser.new
    @parser.title_font = config["title_font"] if config["title_font"]
    @parser.font = config["font"] if config["font"]
    @parser.bold_color = config["bold_color"] if config["bold_color"]
    @parser.align = config["align"].to_sym if config["align"]
    @slide = Rapicco::Slide.new(usakame_h: @rapiko.height)
    @slide.line_margin = config["line_margin"].to_i if config["line_margin"]
    @slide.code_indent = config["code_indent"].to_i if config["code_indent"]
    @slide.bullet = Rapicco::Sprite.new(:bullet, sprite)
    @current_page = -1
    @duration = config["duration"].to_i || 60*30
    @interval = @duration.to_f / @slide.page_w
  rescue => e
    puts e.message
    raise e
  end

  def render_page(page)
    return if page == @current_page
    @file.seek(@positions[page])
    page_data = @file.read(@positions[page + 1] - @positions[page])
    # Note: mruby/c does not support String#each_line
    page_data&.each_line { |line| @parser.parse(line) }
    @slide.render_slide(@parser.dump)
    @current_page = page
    @rapiko.pos = page * 100 / (@positions.size - 2)
    render_usakame
  end

  def prev_page
    page = [0, @current_page - 1].max
    render_page(page || 0)
  end

  def next_page
    page = [@positions.size - 2, @current_page + 1].min
    render_page(page || 0)
  end

  def render_usakame
    @slide.render_sprite(@rapiko)
    @slide.render_sprite(@camerlengo)
  end

  def run
    print "\e[?1049h" # DECSET 1049
    next_page
    start = now = Time.now.to_i
    while true
      500.times do
        sleep_ms 1
        case c = STDIN.read_nonblock(1)&.ord
        when 3 # Ctrl-C
          raise Interrupt
        when 104 # h: prev
          prev_page
        when 108 # l: next
          next_page
        end
      end
      if now + @interval < Time.now.to_i
        camerlengo_pos = (Time.now.to_i - start) * 100 / @duration
        if @camerlengo.pos != camerlengo_pos
          @camerlengo.pos = camerlengo_pos
          render_usakame
        end
        now += @interval
      end
    end
  rescue => e
    puts e.message
  ensure
    @file.close
    print "\e[?25h"   # show cursor
    print "\e[?1049l" # DECRST 1049
  end
end
