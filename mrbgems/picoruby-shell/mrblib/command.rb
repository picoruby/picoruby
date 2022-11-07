class Shell
  class Command
    def initialize
    end

    attr_accessor :feed

    BUILTIN = %w(
      free
      irb
      ruby
      alias
      cd
      echo
      export
      exec
      exit
      fc
      help
      history
      kill
      pwd
      type
      unset
    )

    def find_executable(name)
      ENV['PATH'].each do |path|
        file = "#{path}/#{name}"
        return file if File.exist? file
      end
      return nil
    end

    def exec(params)
      args = params[1, params.length - 1]
      case params[0]
      when "irb"
        _irb(*args)
      when "echo"
        _echo(*args)
      when "type"
        _type(*args)
      when "vim"
        Vim.new(args[0]).start
      when "pwd"
        print Dir.pwd, @feed
      when "cd"
        print @feed if Dir.chdir(args[0] || "/home") != 0
      when "free"
        Object.memory_statistics.each do |k, v|
          print "#{k.to_s.ljust(5)}: #{v.to_s.rjust(8)}", @feed
        end
      else
        if exefile = find_executable(params[0])
          f = File.open(exefile, "r")
          sandbox = Sandbox.new
          ARGV.clear
          args.each_with_index do |param|
            ARGV << param
          end
          mrb = f.read(1024) # TODO: check size
          if mrb[0,8] == "RITE0300"
            begin
              sandbox.exec_mrb(mrb)
              if sandbox.wait && error = sandbox.error
                print "#{error.message} (#{error.class})", @feed
              end
            rescue => e
              # ???
            end
          else
            print "Invalide VM code", @feed
          end
        else
          print "#{params[0]}: command not found", @feed
        end
      end
    end

    #
    # Builtin commands
    #

    def _irb(*params)
      Shell.new.start(:irb)
    end

    def _type(*params)
      params.each_with_index do |name, index|
        print @feed if 0 < index
        if BUILTIN.include?(name)
          print "#{name} is a shell builtin"
        elsif path = find_executable(name)
          print "#{name} is #{path}"
        else
          print "type: #{name}: not found"
        end
      end
      print @feed
    end

    def _echo(*params)
      params.each_with_index do |param, index|
        print " " if 0 < index
        print param
      end
    end

  end
end
