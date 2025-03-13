load "/etc/init.d/r2p2_wifi"

if File.exist?("#{ENV['HOME']}/app.mrb")
  puts "Loading app.mrb"
  load "#{ENV['HOME']}/app.mrb"
elsif File.exist?("#{ENV['HOME']}/app.rb")
  puts "Loading app.rb"
  load "#{ENV['HOME']}/app.rb"
else
  puts "No app.(mrb|rb) found"
end
