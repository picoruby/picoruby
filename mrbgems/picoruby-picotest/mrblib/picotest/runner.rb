# mock for development
class Object
  def self.constants
    [:MyTest, :My2Test]
  end
end

module Picotest
  class Runner
    def self.run(dir, filter = nil)
      $picotest_result = {}
      files = find_tests(dir, filter)
      self.load_tests(files)
      Object.constants.each do |c|
        klass = Object.const_get(c)
        if klass.class? && klass.ancestors.include?(Picotest::Test)
          puts Picotest::RESET
          puts "Running #{c}"
          $picotest_result[c.to_s] = {
            success: 0,
            failure: 0,
            errors: []
          }
          test = klass.new
          tmpfile = "/tmp/#{c.to_s}.rb"
          File.open(tmpfile, "w") do |f|
            f.puts "my_test = #{klass}.new"
            test.list_tests.each do |m|
              f.puts "my_test.#{m}"
            end
          end
          load tmpfile
          puts
        end
      end
      0 < summerize and raise "Test failed"
    end

    # private

    def self.summerize
      total_success = 0
      total_failure = 0
      puts
      puts "Summary"
      puts
      $picotest_result.each do |k, v|
        total_success += v[:success]
        total_failure += v[:failure]
        print (0 < v[:failure] ? RED : GREEN)
        puts "#{k}: success: #{v[:success]}, failure: #{v[:failure]}"
        v[:errors].each do |e|
          puts "  #{e[:method]}: #{e[:error_message]}"
          puts "    expected: #{e[:expected].inspect}"
          puts "    actual:   #{e[:actual].inspect}"
        end
        print RESET
      end
      puts
      total_failure == 0 ? print(GREEN) : print(RED)
      puts "Total: success: #{total_success}, failure: #{total_failure}"
      print RESET
      return total_failure
    end


    def self.load_tests(entries)
      entries.each do |entry|
        if entry.is_a? Array
          load_tests(entry)
        else
          load entry
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
