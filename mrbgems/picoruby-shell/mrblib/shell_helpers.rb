# Helper methods for shell executables
# These are available as Kernel methods in all shell executables

module Kernel
  # mruby-io provides Kernel#gets/print/puts but not Kernel#getc. We add a
  # tiny shim so shell scripts can use the bareword `getc` consistently with
  # `gets`. It mirrors the bareword `gets` in picoruby-machine's kernel.rb:
  # when stdin is the terminal on POSIX, read through the HAL ring buffer
  # (signal-aware, so Ctrl-C interrupts); otherwise delegate to $stdin so
  # input redirection keeps working. Inside a pipeline the PipeIN refinement
  # (mrblib/pipe_refinements.rb) overrides this to read from the inbox
  # Task::Queue instead.
  def getc
    if Machine.posix? && $stdin.fileno == 0
      Machine._stdin_getc
    else
      $stdin.getc
    end
  end

  # Read from files specified in ARGV, or from stdin if ARGV is empty
  # Usage:
  #   each_line_from_files_or_stdin do |line|
  #     print line
  #   end
  def each_line_from_files_or_stdin(&block)
    if Shell::ARGV.empty?
      # Read from stdin
      while line = gets
        block.call(line)
      end
    else
      # Read from files
      i = 0
      while i < Shell::ARGV.size
        filename = Shell::ARGV[i]
        if File.file?(filename)
          File.open(filename, 'r') do |f|
            while line = f.gets
              block.call(line)
            end
          end
        else
          # Raise so the failure flows through Shell.report_exception in the
          # parent shell, giving the same "<message> (<Class>)" format as
          # every other shell-side error. Note: this stops further file
          # arguments from being processed; multi-file cat won't continue
          # past the first missing file. Acceptable trade for now.
          raise "#{filename}: No such file"
        end
        i += 1
      end
    end
  end
end
