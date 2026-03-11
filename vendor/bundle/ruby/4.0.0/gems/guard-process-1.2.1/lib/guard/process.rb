require 'guard/compat/plugin'
require 'spoon'

module Guard
  class Process < Plugin
    def initialize(options = {})
      @pid = nil
      @command = options.fetch(:command)
      @command = @command.split(" ") if @command.is_a?(String)
      @env = options[:env] || {}
      @name = options[:name]
      @dir = options[:dir] || Dir.getwd
      @stop_signal = options[:stop_signal] || "TERM"
      @dont_stop = options[:dont_stop]
      super
    end

    def wait_for_stop?
      @dont_stop
    end

    def process_running?
      begin
        @pid ? ::Process.kill(0, @pid) : false
      rescue Errno::ESRCH => e
        false
      end
    end

    def start
      UI.info("Starting process #{@name}")
      original_env = {}
      @env.each_pair do |key, value|
        original_env[key] = ENV[key]
        ENV[key] = value
      end
      Dir.chdir(@dir) do
        @pid = Spoon.spawnp(*@command)
      end
      original_env.each_pair do |key, value|
        ENV[key] = value
      end
      UI.info("Started process #{@name}")
    end

    def stop
      if @pid
        UI.info("Stopping process #{@name}")
        begin
          ::Process.kill(@stop_signal, @pid)
          ::Process.waitpid(@pid)
        rescue Errno::ESRCH
        end
        @pid = nil
        UI.info("Stopped process #{@name}")
      end
    end

    def wait_for_stop
      if @pid
        UI.info("Process #{@name} is still running...")
        ::Process.waitpid(@pid) rescue Errno::ESRCH
        UI.info("Process #{@name} has stopped!")
      end
    end

    def reload
      if wait_for_stop?
        wait_for_stop
      else
        stop
      end
      start
    end

    def run_all
      true
    end

    def run_on_change(paths)
      reload
    end
  end
end
