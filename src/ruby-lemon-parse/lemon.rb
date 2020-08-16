#! /usr/bin/env ruby

require "open3"

cmd = "lemon/lemon -p #{ARGV[0]} ./parse.y"

puts cmd

out, err = Open3.capture3(cmd)

puts out

errors = err.split("\n")

status = 0

errors.each do |e|
  puts "\e[33;41;1m#{e}\e[m"
  unless e.include?("parsing conflict")
    status = 1
  end
end

exit status
