class Shell
  class Pipeline
    class PipeIO
      def initialize
        @buffer = []
        @remainder = ""
      end

      def puts(str = "")
        @buffer << str.to_s
      end

      def print(str)
        @remainder << str.to_s
      end

      def write(str)
        print(str)
        str.size
      end

      def flush
        self
      end

      def gets
        # First, try to get a complete line from remainder
        if idx = @remainder.index("\n")
          line = @remainder[0..idx-1]
          @remainder = @remainder[idx+1..-1]
          return line
        end

        # If no newline in remainder, check buffer
        if !@buffer.empty?
          return @buffer.shift
        end

        # If remainder has content but no newline, return it all
        if !@remainder.empty?
          line = @remainder
          @remainder = ""
          return line
        end

        # No more data
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

    def initialize(commands)
      @commands = commands
    end

    def exec
      return if @commands.empty?

      if @commands.size == 1
        # Single command - no pipeline needed
        job = Job.new(*@commands[0])
        job.exec
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
        job = Job.new(*cmd_args)
        job.exec
      ensure
        $stdin = old_stdin
        $stdout = old_stdout
      end
    end
  end
end
