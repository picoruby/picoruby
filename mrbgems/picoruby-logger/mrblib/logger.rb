# PicoRuby Simple Logger class implementation
# Usage:
#  logger = Logger.new(level: :info)
#  logger.info("This is an info message")

require 'time'

class Logger
  LOG_LEVELS = [ :debug, :info, :warn, :error, :fatal ]

  def initialize(io_or_filename, level: :info)
    if io_or_filename.is_a?(String)
      @filename = io_or_filename
    elsif io_or_filename.respond_to?(:write)
      @io = io_or_filename
      @filename = ""
    else
      raise ArgumentError, "Invalid argument: must be a filename or IO object"
    end
    @open = true
    update_level(level)
  end

  def close
    if @io&.respond_to?(:close)
      @io.close
    end
    @open = false
  end

  def level=(level_name)
    update_level(level_name)
  end

  def level
    LOG_LEVELS[@level_num]
  end

  private

  def update_level(level_name)
    unless level_num = LOG_LEVELS.index(level_name)
      raise ArgumentError, "Invalid log level: #{level_name}"
    end
    @level_num = level_num
  end

  def method_missing(method_name, *args, &block)
    if level_num = LOG_LEVELS.index(method_name)
      unless @open
        raise IOError, "Logger is closed"
      end
      if @level_num <= level_num
        uptime_sec = Machine.uptime_us / 1000_000
        uptime_min = uptime_sec / 60
        uptime_hour = uptime_min / 60
        uptime_day = uptime_hour / 24
        time_stamp = "#{Time.now.inspect} (uptime: #{uptime_day}d #{uptime_hour % 24}h #{uptime_min % 60}m #{uptime_sec % 60}s)"
        if block
          program_name = args.first
          message = block.call.to_s
        else
          program_name = ""
          message = args.first
        end
        log_message = "[#{time_stamp}] #{method_name.to_s.upcase} -- #{program_name}: #{message}\n"
        if @io
          @io.write log_message
        elsif !@filename.empty?
          File.open(@filename, "a") do |file|
            file.write log_message
          end
        end
        true
      else
        false
      end
    else
      super
    end
  end
end
