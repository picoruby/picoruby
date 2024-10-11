require "vim"

unless ARGV[0]
  puts "No file specified. Usage: vim <file>"
  return
end

Vim.new(ARGV[0]).start
