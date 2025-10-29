class Shell
  class Pipeline
    class PipeIO
      def initialize
        @buffer = []
        @remainder = ""
      end

      def puts(str = "")
        append "#{str}\n"
        nil
      end

      def print(str)
        append str.to_s
        nil
      end

      def write(str)
        print(str)
        str.size
      end

      def flush
        self
      end

      def gets
        if line = @buffer.shift
          return line
        end

        # If remainder has content, return it all
        if !@remainder.empty?
          line = @remainder
          @remainder = ""
          return line
        end

        # No data
        nil
      end

      def each_line
        while line = gets
          yield line
        end
      end

      def empty?
        @buffer.empty? && @remainder.empty?
      end

      def clear
        @buffer.clear
        @remainder = ""
      end

      private

      def append(str)
        while idx = str.index("\n")
          line = @remainder + (str[0..idx] || "")
          @buffer << line
          str = str[(idx + 1)..-1] || ""
          @remainder = ""
        end
        @remainder += str
      end
    end

    def self.parse(command_line)
      commands = []
      current = []

      # First, split by pipe character
      segments = command_line.split("|")

      segments.each do |segment|
        # Trim and split each segment into arguments
        args = segment.strip.split(" ")
        commands << args unless args.empty?
      end

      commands
    end

    def initialize(commands, pool: nil)
      @commands = commands
      @pool = pool
    end

    def exec
      return if @commands.empty?

      if @commands.size == 1
        # Single command - no pipeline needed
        job = Job.new(*@commands[0], pool: @pool)
        job.exec
        job.release_sandbox
        return
      end

      # Pipeline execution
      input = nil
      @commands.each_with_index do |cmd_args, idx|
        is_first = (idx == 0)
        is_last = (idx == @commands.size - 1)

        if is_first
          # First command: capture output
          output = PipeIO.new
          execute_with_redirect(cmd_args, nil, output)
          input = output
        elsif is_last
          # Last command: use captured input, normal output
          execute_with_redirect(cmd_args, input, nil)
        else
          # Middle command: use captured input, capture output
          output = PipeIO.new
          execute_with_redirect(cmd_args, input, output)
          input = output
        end
      end
    end

    private

    def execute_with_redirect(cmd_args, input, output)
      old_stdin = $stdin
      old_stdout = $stdout

      begin
        $stdin = input if input
        $stdout = output if output

        # Execute command using Job (which uses Sandbox)
        job = Job.new(*cmd_args, pool: @pool)
        job.exec
        # Release sandbox immediately after pipeline stage completes
        job.release_sandbox
      ensure
        $stdin = old_stdin
        $stdout = old_stdout
      end
    end
  end
end
