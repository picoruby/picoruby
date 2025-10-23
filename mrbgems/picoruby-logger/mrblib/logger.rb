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

  def initialize(io_or_filename, level: :info)
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
        time_stamp = "#{Time.now.inspect} (uptime: #{Machine.uptime_formatted})"
        if block
          program_name = args.first
          message = block.call.to_s
        else
          program_name = ""
          message = args.first
        end
        @io.write "[#{time_stamp}] #{method_name.to_s.upcase} -- #{program_name}: #{message}\n"
        @io.fsync if @fsync_supported
        true
      else
        false
      end
    else
      super
    end
  end
end
