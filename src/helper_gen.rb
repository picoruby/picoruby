#! /usr/bin/env ruby

def generate_tokenizer_helper
  File.open("tokenizer_helper.h", "w") do |file|
    file.puts <<~TEXT
      inline static char *tokenizer_state_name(State n)
      {
        switch(n) {
    TEXT
    File.open("token.h", "r") do |f|
      f.each_line do |line|
        break if line.match?("enum state")
      end
      f.each_line do |line|
        break if line.match?("State")
        data = line.match(/\A\s+(\w+)\s+/)
        if data
          file.puts "    case(#{data[1]}): return \"#{data[1].ljust(12)}\";"
        end
      end
    end
    file.puts <<~TEXT
          default: return "\\e[37;41;1m\\\"UNDEFINED    \\\"\\e[m";
        }
      }
    TEXT

    file.puts ""

    file.puts <<~TEXT
      inline static char *tokenizer_mode_name(Mode n)
      {
        switch(n) {
    TEXT
    File.open("tokenizer.h", "r") do |f|
      f.each_line do |line|
        break if line.match?("enum mode")
      end
      f.each_line do |line|
        break if line.match?("Mode")
        data = line.match(/\A\s+(\w+)/)
        if data
          file.puts "    case(#{data[1]}): return \"#{data[1].rjust(19)}\";"
        end
      end
    end
    file.puts <<~TEXT
          default: return "\\e[37;41;1m\\\"UNDEFINED         \\\"\\e[m";
        }
      }
    TEXT
  end
end

case ARGV[0]
when "tokenizer"
  generate_tokenizer_helper
else
  p ARGV
  raise "Argument error!"
end
