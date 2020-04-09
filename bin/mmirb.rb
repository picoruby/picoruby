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
      puts 'mmirb client started.'
      while line = Reline.readline("mmirb> ", true)
        case line.chomp
        when 'exit', 'quit'
          break
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
    rescue Interrupt
      puts '^C'
      `stty #{stty_save}` if stty_save
      exit 0
    end
  end
end

TMPDIR = Dir.tmpdir
PID_SOCAT = TMPDIR + "/mmirb_socat.pid"
PID_SERVER = TMPDIR + "/mmirb_server.pid"
SOCAT_OUTPUT = TMPDIR + "/socat_output"

`socat -d -d pty,raw,echo=0 pty,raw,echo=0 </dev/null > #{SOCAT_OUTPUT} 2>&1 & echo $! > #{PID_SOCAT}`
sleep 1

File.open(SOCAT_OUTPUT, "r") do |f|
  fd_server = f.gets.split(" ").last
  fd_client = f.gets.split(" ").last
  systemu("../cli/mmirb #{fd_server}") do |cid|
    Client.start(fd_client)
    Process.kill 9, cid
  end
end

`cat #{PID_SOCAT} | xargs kill -9`
