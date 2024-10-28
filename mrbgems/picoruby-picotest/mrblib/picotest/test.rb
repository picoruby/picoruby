module Picotest

  class Test
    def initialize
      @doubles = []
    end

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

    def assert_nil(obj)
      report(obj.nil?, "Expected #{obj} to be nil", nil, obj)
    end

    def assert_equal(expected, actual)
      report(expected == actual, "Expected #{expected} but got #{actual}", expected, actual)
    end

    def asser_not_equal(expected, actual)
      report(expected != actual, "Expected #{expected} to be different from #{actual}", expected, actual)
    end

    def assert_raise(exception, message = nil, &block)
      begin
        block.call
        report(false, "Expected #{exception} but nothing raised", exception, nil)
      rescue => e
        if e == exception && (message.nil? || (message && e.message == message))
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

    # private

    def report(result, error_message, expected, actual)
      method = caller(2, 1)[0]
      klass_name = self.class.to_s
      if result
        print "#{Picotest::GREEN}.#{Picotest::RESET}"
        $picotest_result[klass_name][:success] += 1
      else
        print "#{Picotest::RED}F#{Picotest::RESET}"
        $picotest_result[klass_name][:failure] += 1
        $picotest_result[klass_name][:errors] << {
          method: method,
          error_message: error_message,
          expected: expected,
          actual: actual
        }
      end
    end
  end
end
