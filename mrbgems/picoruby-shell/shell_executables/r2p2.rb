
begin
  require "cyw43"
  puts 1
  if CYW43.respond_to?(:enable_sta_mode)
    ENV['WIFI_MODULE'] = "cwy43"
    pin = GPIO.new(22, GPIO::IN|GPIO::PULL_UP)
    if pin.low?
      system "nmble"
    end
    system "wifi_connect --check-auto-connect"
  end
rescue LoadError
  # No WiFi module
end

if File.exist?("/home/app.mrb")
  puts "Loading app.mrb"
  load "/home/app.mrb"
elsif File.exist?("/home/app.rb")
  puts "Loading app.rb"
  load "/home/app.rb"
else
  puts "No app.(mrb|rb) found"
end
