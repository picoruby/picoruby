#class String
#  def chop!
#    self = self[0, self.size - 1]
#    self
#  end
#  def chop
#    self.dup.chop!
#  end
#end

# abandon buffer
while !fd_empty? do
  c = getc
  print c
end

puts "pid: #{pid}" # client will receive the pid
line = "" # String.new does not work...?
print "mmirb> "
while true
  suspend_task # suspend task itself
  while !fd_empty? do
    c = getc
    break if c == nil
    case c.ord
    when 7 # ESC
      exit_shell
    when 9 # horizontal tab \t
      # ignore
    when 10 # LF \n
      # ignore
    when 13  # CR \r
      print "\r\n"
      case line.chomp
      when ""
        # skip
      when "quit", "exit"
        exit_shell
      else
        unless compile_and_run(line)
          puts "Failed to compile!"
        end
      end
      print "mmirb> "
      line = ""
    when 65, 66, 67, 68, 126 # ↑↓→←etc.
      # ignore?
    when 127 # backspace
      if line.size > 0
        line = line[0, line.size - 1] # line.chop!
        print "\b \b"
      end
    else
      print c
      # print "\r\n#{c.ord}\r\n"
      line << c
    end
  end
end
