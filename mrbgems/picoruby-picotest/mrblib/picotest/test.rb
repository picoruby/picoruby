module Picotest

  class Test

    def self.description(text)
    end

    def initialize
      @klass_name = self.class.to_s
      @doubles = []
      @result = {
         "success_count" => 0,
         "failures" => [],
         "exceptions" => [],
         "crashes" => []
      }
    end

    attr_reader :result

    def list_tests
      result = []
      self.methods.each do |m|
        if m.to_s.start_with?('test_')
          result.unshift(m)
        end
      end
      result
    end

    # stub(obj).method_name().returns(return_value)
    def stub(doubled_obj)
      double = Picotest::Double._init(:stub,  doubled_obj)
      @doubles << double
      double
    end

    def mock(doubled_obj)
      double = Picotest::Double._init(:mock, doubled_obj)
      @doubles << double
      double
    end

    def stub_any_instance_of(klass)
      if klass.class?
        double = Picotest::Double._init(:stub, klass, any_instance_of: true)
        @doubles << double
        double
      else
        raise TypeError, "Argument must be a class"
      end
    end

    def mock_any_instance_of(klass)
      if klass.class?
        double = Picotest::Double._init(:mock, klass, any_instance_of: true)
        @doubles << double
        double
      else
        raise TypeError, "Argument must be a class"
      end
    end

    def mock_methods
      #todo
    end

    def clear_doubles
      @doubles.each do |double|
        double.remove_singleton
      end
    end

    def setup
    end

    def teardown
    end

    def assert(result)
      report(result, "Expected truthy but got falsy", nil, nil)
    end
    alias assert_true assert

    def assert_false(result)
      report(!result, "Expected falsy but got truthy", nil, nil)
    end

    def assert_nil(obj)
      report(obj.nil?, "Expected #{obj} to be nil", nil, obj)
    end

    def assert_equal(expected, actual)
      report(expected == actual, "Expected #{expected} but got #{actual}", expected, actual)
    end

    def assert_not_equal(expected, actual)
      report(expected != actual, "Expected #{expected} to be different from #{actual}", expected, actual)
    end

    def assert_raise(*exceptions, &block)
      if exceptions.empty?
        raise ArgumentError, "At least one exception class must be given"
      end
      message = if 1 < exceptions.size && exceptions[-1].is_a?(String)
        exceptions.pop
      end
      unless exceptions.all? { |e| e.class? }
        raise TypeError, "All arguments must be classes"
      end
      exception = exceptions.join(' || ')
      begin
        block.call
        report(false, "Expected #{exception} but nothing raised", exception, nil)
      rescue => e
        if exceptions.include?(e.class) && (message.nil? || e.message == message)
          report(true, nil, nil, nil)
        else
          if message
            report(false, "Expected `#{message} (#{exception})` but got `#{e.message} (#{e.class})`", message, e)
          else
            report(false, "Expected #{exception} but got #{e.class}", exception, e.class)
          end
        end
      end
    end

    def assert_in_delta(expected, actual, delta = 0.001)
      report((expected - actual).abs < delta, "Expected #{expected} but got #{actual}", expected, actual)
    end

    def exception_report(data)
      @result["exceptions"] << data
    end

    alias report_exception exception_report

    # private

    def report(result, error_message, expected, actual)
      method = caller(2, 1)[0]
      if result
        print "#{Picotest::GREEN}.#{Picotest::RESET}"
        @result["success_count"] += 1
      else
        print "#{Picotest::RED}F#{Picotest::RESET}"
        @result["failures"] << {
          method: method,
          error_message: error_message,
          expected: expected,
          actual: actual
        }
      end
    end
  end
end
