# PicoRuby Simple Logger class implementation
# Usage:
#  logger = Logger.new("filename.log", level: :info)
#  logger.info("This is an info message")
#  logger.debug("This is a debug message") # Will not be logged if level is :info
#  logger.fatal("Unrecoverable error occurred!")
#  logger.close

require 'time'

class Logger
  LOG_LEVELS = [ :debug, :info, :warn, :error, :fatal ]
  DEFAULT_BUFFER_MAX = 32

  def initialize(io_or_filename, level: :info, buffer_max: DEFAULT_BUFFER_MAX)
    @buffer_max = buffer_max
    @buffer = []
    if io_or_filename.is_a?(String)
      @io = File.open(io_or_filename, "a")
    elsif io_or_filename.respond_to?(:write)
      @io = io_or_filename
    else
      raise ArgumentError, "Invalid argument: must be a filename or IO object"
    end
    @fsync_supported = @io.respond_to?(:fsync)
    @open = true
    update_level(level)
    update_flush_level(:error)
  end

  def close
    @io.close if @io.respond_to?(:close)
    @open = false
  end

  def level=(level_name)
    update_level(level_name)
  end

  def level
    LOG_LEVELS[@level_num]
  end

  def flush_level=(level_name)
    update_flush_level(level_name)
  end

  def flush_level
    LOG_LEVELS[@flush_level_num]
  end

  def flush
    return unless @open
    while buf = @buffer.shift
      @io.write buf
      @io.write "\n"
      @io.fsync if @fsync_supported
    end
  end

  private

  def update_level(level_name)
    unless level_num = LOG_LEVELS.index(level_name)
      raise ArgumentError, "Invalid log level: #{level_name}"
    end
    @level_num = level_num
  end

  def update_flush_level(level_name)
    unless level_num = LOG_LEVELS.index(level_name)
      raise ArgumentError, "Invalid flush level: #{level_name}"
    end
    @flush_level_num = level_num
  end

  def method_missing(method_name, *args, &block)
    if level_num = LOG_LEVELS.index(method_name)
      unless @open
        raise IOError, "Logger is closed"
      end
      if @level_num <= level_num
        if block
          message = "#{args.first}: #{block.call.to_s}"
        else
          message = args.first || ''
        end
        log "#{Machine.uptime_formatted},#{method_name.to_s.upcase[0]},#{message.chomp}"
        flush if @flush_level_num <= level_num
        true
      else
        false
      end
    else
      super
    end
  end

  def log(entry)
    if @io != STDOUT
      @buffer << entry
      @buffer.shift if @buffer_max <= @buffer.size
    end
    STDOUT.puts entry
  end
end
