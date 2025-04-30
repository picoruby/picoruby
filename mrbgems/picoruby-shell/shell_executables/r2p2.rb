if ENV['WIFI_MODULE'] == "cwy43"
  if Shell.get_device(:gpio, 'TRIGGER_NMBLE').low?
    system "nmble"
  end
  system "wifi_connect --check-auto-connect"
end

if File.exist?("#{ENV['HOME']}/app.mrb")
  puts "Loading app.mrb"
  load "#{ENV['HOME']}/app.mrb"
elsif File.exist?("#{ENV['HOME']}/app.rb")
  puts "Loading app.rb"
  load "#{ENV['HOME']}/app.rb"
else
  puts "No app.(mrb|rb) found"
end
