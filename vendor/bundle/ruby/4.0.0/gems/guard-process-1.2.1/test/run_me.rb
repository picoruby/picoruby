#!/usr/bin/env ruby

file_name = "#{File.expand_path(File.dirname(__FILE__))}/test_environment.txt"

File.open(file_name, 'w') do |file|
  file.write("VAR1 = #{ENV['VAR1']}\n")
  file.write("VAR2 = #{ENV['VAR2']}\n")
  file.write("VAR3 = #{ENV['VAR3']}\n")
end

loop do
  sleep 1
end

