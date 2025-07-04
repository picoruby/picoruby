# 1. Load picotest framework
puts "Loading picotest..."
require 'picotest'
puts "picotest loaded."

# 2. Load the test file directly using an absolute path
test_file_path = "/home/hasumi/work/R2P2/lib/picoruby/mrbgems/picoruby-base64/test/test_base64"
puts "Loading #{test_file_path}..."
begin
  require test_file_path
  puts "#{test_file_path} loaded."
rescue => e
  puts "Failed to load #{test_file_path}: #{e.message}"
end

# 3. Check for TestBase64 class
begin
  TestBase64
rescue NameError
  puts "FATAL: TestBase64 class not found after require!"
  # exit is not available, so just stop here
  raise "Stopping execution"
end

# 4. Run the tests
puts "Running TestBase64 manually..."
test = TestBase64.new
begin
  puts; print "  TestBase64#test_encode "; test.setup; test.test_encode; test.teardown
  puts; print "  TestBase64#test_decode "; test.setup; test.test_decode; test.teardown
  puts; print "  TestBase64#test_long_string "; test.setup; test.test_long_string; test.teardown
  puts
rescue => e
  puts "\nEXCEPTION during test execution: #{e.class}: #{e.message}"
end

# 5. Manually summarize the result
result = test.result
puts
puts "Summary for TestBase64:"
puts "  success: #{result['success_count']}, failure: #{result['failures'].size}, exception: #{result['exceptions'].size}"
result['failures'].each do |f|
  puts "  FAILURE: #{f['method']}: #{f['error_message']}"
end
