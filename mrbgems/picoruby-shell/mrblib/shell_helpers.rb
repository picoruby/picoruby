# Helper methods for shell executables
# These are available as Kernel methods in all shell executables

module Kernel
  # Read from files specified in ARGV, or from stdin if ARGV is empty
  # Usage:
  #   each_line_from_files_or_stdin do |line|
  #     print line
  #   end
  def each_line_from_files_or_stdin(&block)
    if ARGV.empty?
      # Read from stdin
      while line = gets
        block.call(line)
      end
    else
      # Read from files
      ARGV.each do |filename|
        if File.file?(filename)
          File.open(filename, 'r') do |f|
            f.each_line do |line|
              block.call(line)
            end
          end
        else
          $stderr.puts "#{File.basename($0)}: #{filename}: No such file"
        end
      end
    end
  end
end
