module Picotest
  class Test
    def list_tests
      result = []
      self.methods.each do |m|
        if m.to_s.start_with?('test_')
          result.unshift(m)
        end
      end
      result
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
