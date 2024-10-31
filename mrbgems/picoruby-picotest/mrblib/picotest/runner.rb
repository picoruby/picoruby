module Picotest
  class Runner
    def self.run(dir, filter = nil)
      $picotest_load_crashes = []
      $picotest_result = {}
      files = find_tests(dir, filter)
      self.load_tests(files)
      Object.constants.each do |c|
        klass = Object.const_get(c)
        if klass.class? && klass.ancestors.include?(Picotest::Test)
          puts Picotest::RESET
          puts "Running #{c}"
          $picotest_result[c.to_s] = {
            success_count: 0,
            failures: [],
            exceptions: [],
            crashes: []
          }
          test = klass.new
          tmpfile = "/tmp/#{c.to_s}.rb"
          File.open(tmpfile, "w") do |f|
            f.puts "my_test = #{klass}.new"
            test.list_tests.each do |t|
              f.puts
              f.puts "my_test.setup"
              f.puts "begin"
              f.puts "  my_test.#{t}"
              f.puts "rescue => e"
              f.puts "  $picotest_result['#{c.to_s}'][:exceptions] << {method: '#{t}', raise_message: e.message}"
              f.puts "end"
              f.puts "my_test.teardown"
              f.puts "my_test.clear_doubles"
            end
          end
          begin
            load tmpfile
          rescue => e
            $picotest_result[c.to_s][:crashes] << e.message
          end
          puts
        end
      end
      0 < summarize and raise "Test failed"
    end

    # private

    def self.summarize
      total_success = 0
      total_failure = 0
      total_exception = 0
      total_crash = 0
      puts
      puts "Summary"
      puts
      $picotest_result.each do |k, v|
        total_success += v[:success_count]
        failure_count = v[:failures].size
        exception_count = v[:exceptions].size
        crash_count = v[:crashes].size
        error_count = failure_count + exception_count + crash_count
        total_failure += failure_count
        total_exception += exception_count
        total_crash += crash_count
        print (0 < error_count ? Picotest::RED : Picotest::GREEN)
        puts "#{k}: success: #{v[:success_count]}, failure: #{failure_count}, exception: #{exception_count}, crash: #{crash_count}"
        v[:failures].each do |e|
          puts "  #{e[:method]}: #{e[:error_message]}"
          puts "    expected: #{e[:expected].inspect}"
          puts "    actual:   #{e[:actual].inspect}"
        end
        v[:exceptions].each do |e|
          puts "  #{e[:method]}: #{e[:raise_message]}"
        end
        v[:crashes].each do |e|
          puts "  crash: #{e}"
        end
        print Picotest::RESET
      end
      $picotest_load_crashes.each do |e|
        print Picotest::RED
        puts "Crash in loading: #{e[:entry]}: #{e[:message]}"
        print Picotest::RESET
      end
      puts
      total_error_count = total_failure + total_exception + total_crash + $picotest_load_crashes.size
      total_error_count == 0 ? print(Picotest::GREEN) : print(Picotest::RED)
      puts "Total: success: #{total_success}, failure: #{total_failure}, exception: #{total_exception}, crash: #{total_crash}, crash in loading: #{$picotest_load_crashes.size}"
      print Picotest::RESET
      return total_error_count
    end


    def self.load_tests(entries)
      entries.each do |entry|
        if entry.is_a? Array
          load_tests(entry)
        else
          begin
            load entry
          rescue => e
            $picotest_load_crashes << {
              entry: entry,
              message: e.message
            }
          end
        end
      end
    end

    def self.find_tests(dir, filter)
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
