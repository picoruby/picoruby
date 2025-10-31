# Usage:
#  logger = RotateLogger.new(interval: 60, basename: 'APP', ext: 'LOG')
#  logger.info "message"

class RotateLogger
  def initialize(dir:, basename: '', ext: 'log', interval: 3600, keep_size: 30)
    @interval = interval
    @dir = dir
    @keep_size = keep_size
    @basename = basename
    @ext = ext
    @timestamp = Time.now.to_i
    delete_old_logs
    open
  end

  private

  def method_missing(name, *args, &block)
    rotate
    case name
    when :debug
      @logger.debug(args[0], &block)
    when :info
      @logger.info(args[0], &block)
    when :warn
      @logger.warn(args[0], &block)
    when :error
      @logger.error(args[0], &block)
    when :fatal
      @logger.fatal(args[0], &block)
    when :close
      @logger.close
    when :flush
      @logger.flush
    end
  end

  def open
    @logger = Logger.new(new_name)
  end

  def new_name
    t = Time.now
    fname = sprintf("%s%04d%02d%02d-%02d%02d%02d.%s",
                    @basename,
                    t.year, t.mon, t.mday, t.hour, t.min, t.sec,
                    @ext)
    "#{@dir}/#{fname}"
  end

  def rotate
    return if Time.now.to_i < @timestamp + @interval
    @logger.close
    @timestamp = Time.now.to_i
    open
    delete_old_logs
  end

  def delete_old_logs
    return if @keep_size.nil? || @keep_size <= 0
    logs = []
    Dir.open(@dir) do |dir|
      while entry = dir.read
        if File.file?("#{@dir}/#{entry}")
          if (@basename.empty? || entry.start_with?(@basename)) && entry.end_with?(".#{@ext}")
            logs << entry
          end
        end
      end
    end
    logs.sort!
    (logs.size - @keep_size).times do
      File.unlink("#{@dir}/#{logs.shift}")
    end
  end
end
