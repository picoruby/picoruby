#!/usr/bin/env ruby

require "bundler"
Bundler.require

class Client
  def self.start(fd)
    serialport = Serial.new(fd)
    begin
      stty_save = `stty -g`.chomp
    rescue
    end
    begin
      puts 'mmirb started.'
      while line = Reline.readline("mmirb> ", true)
        case line.chomp
        when 'exit', 'quit'
          break
        when /\A\s*\z/
          # next
        else
          serialport.write line
          sleep 0.1
          while true
            result = serialport.read(1024)
            if result == ""
              break
            else
              puts result
              sleep 0.1
            end
          end
        end
      end
    ensure
      puts '^C'
      `stty #{stty_save}` if stty_save
      exit 0
    end
  end
end

TMPDIR = Dir.tmpdir
PID_SOCAT = TMPDIR + "/mmirb_socat.pid"
SOCAT_OUTPUT = TMPDIR + "/socat_output"

begin
  `socat -d -d pty,raw,echo=0 pty,raw,echo=0 </dev/null > #{SOCAT_OUTPUT} 2>&1 & echo $! > #{PID_SOCAT}`
  pid = `cat #{PID_SOCAT}`.to_i
  sleep 1
  File.open(SOCAT_OUTPUT, "r") do |f|
    fd_server = f.gets.split(" ").last
    fd_client = f.gets.split(" ").last
    systemu("../cli/mmirb #{fd_server}") do |cid|
      puts "client pid: #{Process.pid}"
      puts "socat  pid: #{pid}"
      puts "server pid: #{cid}"
      Client.start(fd_client)
      Process.kill 9, cid
    end
  end
ensure
  Process.kill 9, pid
end
