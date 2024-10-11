load "/etc/init.d/r2p2_wifi"

if File.exist?("/home/app.mrb")
  puts "Loading app.mrb"
  load "/home/app.mrb"
elsif File.exist?("/home/app.rb")
  puts "Loading app.rb"
  load "/home/app.rb"
else
  puts "No app.(mrb|rb) found"
end
