require 'yaml'
require 'terminus'
begin
  require 'shinonome'
rescue LoadError
  # ignore. mayby MicroRuby
end

class Rapicco
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

  def initialize(path)
    @path = path
    refresh
  rescue => e
    puts e.message
    raise e
  end

  def run
    print "\e[?25l" # hide cursor
    print "\e[?1049h" # DECSET 1049
    print "\e[2J" # clear screen
    next_page
    start = now = Time.now.to_i
    while true
      100.times do
        sleep_ms 1
        begin
          c = STDIN.read_nonblock(1)&.ord
        rescue Interrupt
          return
        rescue SignalException => e
          if e.message == "SIGTSTP"
            print "\e[?25h"   # show cursor
            print "\e[?1049l" # DECRST 1049
            puts "\nSuspended"
            Signal.trap(:CONT) do
              print "\e[?25l" # hide cursor
              print "\e[?1049h" # DECSET 1049
              ENV['SIGNAL_SELF_MANAGE'] = 'yes'
            end
            Signal.raise(:TSTP)
          end
        end
        case c
        when 12 # Ctrl-L
          @slide.get_screen_size
          current_page = @current_page
          refresh
          @current_page = current_page - 1
          next_page
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
  ensure
    @file.close
    print "\e[?25h"   # show cursor
    print "\e[?1049l" # DECRST 1049
    puts "Interrupted"
    puts "Error: #{e.message}" if e
  end

  private

  def render_page(page)
    return if page == @current_page
    @file.seek(@positions[page])
    page_data = @file.read(@positions[page + 1] - @positions[page])
    page_data&.each_line { |line| @parser.parse(line) }
    lines = @parser.dump
    @slide.render_slide(lines)
    @current_page = page
    @rapiko.pos = page * 100 / (@positions.size - 2)
    render_usakame
    if note = lines[-1][:note]
      render_note(note)
    end
  end

  def render_note(note)
    print COLORS[@note_color],
            "\e[#{@slide.page_h};1H",
            note,
            "\e[0K", # clear to end of line
            COLORS[:reset]
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

  def refresh
    yaml = ""
    @positions = []
    raise "File not found: #{@path}" unless File.file?(@path)
    @file.close unless @file.nil?
    @file = File.open(@path, 'r')
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
    sprite_author = config["sprite"] || "hasumikin"
    @rapiko = Rapicco::Sprite.new(:rapiko, sprite_author)
    @camerlengo = Rapicco::Sprite.new(:camerlengo, sprite_author)
    @parser = Rapicco::Parser.new
    @parser.title_font = config["title_font"] if config["title_font"]
    @parser.font = config["font"] if config["font"]
    @parser.bold_color = config["bold_color"] if config["bold_color"]
    @parser.align = config["align"].to_sym if config["align"]
    @slide = Rapicco::Slide.new(usakame_h: @rapiko.height)
    @slide.line_margin = config["line_margin"].to_i if config["line_margin"]
    @slide.code_indent = config["code_indent"].to_i if config["code_indent"]
    @slide.bullet = Rapicco::Sprite.new(:bullet, sprite_author)
    @current_page = -1
    @duration = config["duration"].to_i || 60*30
    @interval = @duration.to_f / @slide.page_w
    @note_color = config["note_color"]&.to_sym || :white
  end

end
