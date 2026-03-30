if Machine.wifi_available?
  if Shell.get_device(:gpio, 'TRIGGER_NMBLE')&.low?
    load "/bin/nmble"
  end
  ARGV[0] = "--check-auto-connect"
  load "/bin/wifi_connect"
  ARGV.clear
end

begin
  if File.exist?("#{ENV['HOME']}/app.mrb")
    puts "Loading app.mrb"
    load "#{ENV['HOME']}/app.mrb"
  elsif File.exist?("#{ENV['HOME']}/app.rb")
    puts "Loading app.rb"
    load "#{ENV['HOME']}/app.rb"
  else
    require 'dfu'
    app = DFU::BootManager.resolve
    if app
      puts "Loading #{app}"
      load app
    else
      puts "No app found"
    end
  end
rescue => e
  puts "Error loading app: #{e}"
end
