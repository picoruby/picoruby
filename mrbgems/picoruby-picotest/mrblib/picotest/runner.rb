# Picotest::Runner - Test orchestrator (CRuby only).
# Discovers test files, generates temporary scripts, spawns the target VM
# (PicoRuby/MicroRuby) as a subprocess, and parses JSON results from stdout.

module Picotest
  class Runner

    TMPDIR = "/tmp"
    SEPARATOR = "----\n"

    def initialize(dir, filter = nil, tmpdir = TMPDIR, require_name = nil, load_files = [], load_path = nil)
      unless dir.start_with? "/"
        dir = File.join Dir.pwd, dir
      end
      puts "Running tests in #{dir}"
      @load_path = load_path
      @tmpdir = tmpdir
      @require_name = require_name
      @load_files = load_files
      @entries = find_tests(dir, filter)
      @result = {}
      @test_classes = []
      @load_crashes = []
      # See README.md about ENV["RUBY"]
      @ruby_path = ENV["RUBY"] || RbConfig.ruby
      if @ruby_path.nil?
        raise "RbConfig.ruby is nil. Maybe running on baremetal?"
      end
    end

    def run
      load_tests(@entries)
      return summarize
    ensure
      @test_classes.each do |klass|
        Object.send(:remove_const, klass.to_s) if Object.const_defined?(klass.to_s)
      end
    end

    # private

    def run_test(entry)
      test_classes = Object.constants.select{|c| c.to_s.end_with?('Test')}.map{|c| Object.const_get(c)}
      test_classes.reject! do |c|
        !c.class? || !c.ancestors.include?(Picotest::Test) || @test_classes.include?(c)
      end
      @test_classes += test_classes
      test_classes.each do |klass|
        test = klass.new
        tmpfile = "#{@tmpdir}/#{klass.to_s}.rb"
        File.open(tmpfile, "w") do |f|
          f.puts <<~KERNEL
            module Kernel
              alias :require_original :require
              def require(name)
                return require_original(name)
              rescue LoadError => e
                # ignore LoadError
              end
            end
          KERNEL
          f.puts "require 'picotest' # pre-built gem"
          f.puts "$LOAD_PATH = ['#{@load_path}']" if @load_path
          f.puts "\n# implementation and mock"
          @load_files.each do |file|
            f.puts "load '#{file}'"
          end
          if @require_name
            f.puts "\n# library to require"
            f.puts "require '#{@require_name}'"
          end
          f.puts "\n# test file to load"
          f.puts "load '#{entry}'"
          f.puts "\nmy_test = #{klass}.new"
          f.puts "puts"
          f.puts "print 'From #{entry}:'"
          test.list_tests.each do |t|
            f.puts "puts"
            f.puts "print '  #{klass.to_s}##{t} '"
            f.puts "my_test.setup"
            f.puts "begin"
            f.puts "  my_test.#{t}"
            f.puts "rescue => e"
            f.puts "  my_test.report_exception({method: '#{t}', raise_message: e.message})"
            f.puts "end"
            f.puts "my_test.teardown"
            f.puts "my_test.clear_doubles"
          end
          f.puts "puts"
          f.print 'puts "', SEPARATOR, '"', "\n"
          f.puts "puts JSON.generate(my_test.result)"
        end
        error_file = "#{@tmpdir}/#{klass.to_s}_error"
        File.open(error_file, "w") do |error|
          cmd = "#{@ruby_path} #{tmpfile}"
          puts "Running: #{cmd}"
          IO.popen(cmd, err: error.fileno) do |io|
            outputs = io.read&.split("----\n") || []
            print Picotest::RESET
            puts outputs[0]
            print Picotest::RESET
            unless outputs[1].nil?
              @result[klass.to_s] = JSON.parse(outputs[1])
            end
          end
        end
        if $?.exitstatus != 0
          raise "Crash in running #{klass.to_s} tests. See #{error_file} for details."

          if FileTest.size?(error_file)
            @result[klass.to_s] = {
              "success_count" => 0,
              "failures" => [],
              "exceptions" => [],
              "crashes" => []
            }
            File.open(error_file) do |error|
              @result[klass.to_s]["crashes"] << error.read
            end
          end
        end
      end
    end

    def summarize
      total_success = 0
      total_failure = 0
      total_exception = 0
      total_crash = 0
      puts
      puts "Summary"
      puts
      @result.each do |k, v|
        total_success += v["success_count"]
        failure_count = v["failures"].size
        exception_count = v["exceptions"].size
        crash_count = v["crashes"].size
        error_count = failure_count + exception_count + crash_count
        total_failure += failure_count
        total_exception += exception_count
        total_crash += crash_count
        print (0 < error_count ? Picotest::RED : Picotest::GREEN)
        puts "#{k}:"
        puts "  success: #{v["success_count"]}, failure: #{failure_count}, exception: #{exception_count}, crash: #{crash_count}"
        v["failures"].each do |e|
          puts "  #{e["method"]}: #{e["error_message"]}"
          puts "    expected: #{e["expected"].inspect}"
          puts "    actual:   #{e["actual"].inspect}"
        end
        v["exceptions"].each do |e|
          puts "  #{e["method"]}: #{e["raise_message"]}"
        end
        v["crashes"].each do |crash|
          puts "  crash:"
          crash.split("\n").each do |line|
            puts "    #{line}"
          end
        end
        print Picotest::RESET
      end
      @load_crashes.each do |crash|
        print Picotest::RED
        puts "Crash in loading: #{crash[:entry]}: #{crash[:message]}"
        print Picotest::RESET
      end
      puts
      total_error_count = total_failure + total_exception + total_crash + @load_crashes.size
      total_error_count == 0 ? print(Picotest::GREEN) : print(Picotest::RED)
      puts "Total: success: #{total_success}, failure: #{total_failure}, exception: #{total_exception}, crash: #{total_crash}, crash in loading: #{@load_crashes.size}"
      puts Picotest::RESET
      return total_error_count
    end

    def load_tests(entries)
      entries.each do |entry|
        if entry.is_a? Array
          load_tests(entry)
        else
          begin
            load entry
            run_test(entry)
          rescue => e
            @load_crashes << {
              entry: entry,
              message: "#{e.class}: #{e.message}"
            }
          end
        end
      end
    end

    def find_tests(dir, filter)
      files = []
      Dir.foreach(dir) do |entry|
        next if entry == "." or entry == ".."
        next unless entry.end_with? "_test.rb"
        entry = File.join dir, entry
        if FileTest.directory?(entry)
          files << find_tests(entry, filter)
        else
          files << entry if filter.nil? || File.basename(entry).include?(filter)
        end
      end
      files
    end
  end
end
