# `require "pre-built-gem"` is simply ignored in MicroRuby but PicoRuby needs it
require "picotest"

# For MicroRuby, gems are pre-loaded, but the test file itself is not.

# 1. Load the test file directly using an absolute path
test_file_path = "/home/hasumi/work/R2P2/lib/picoruby/mrbgems/picoruby-base64/test/test_base64"
require test_file_path

# 2. Run the tests
test = TestBase64.new
begin
  # Manually call each test method
  test.setup; test.test_encode; test.teardown
  test.setup; test.test_decode; test.teardown
  test.setup; test.test_long_string; test.teardown
rescue => e
  puts "\nEXCEPTION during test execution: #{e.class}: #{e.message}"
end

# 3. Manually summarize the result
result = test.result
puts
puts "Summary for TestBase64:"
puts "  success: #{result['success_count']}, failure: #{result['failures'].size}, exception: #{result['exceptions'].size}"
result['failures'].each do |f|
  puts "  FAILURE: #{f[:method]}"
  puts "  EXPECTED: #{f[:expected]}"
  puts "  ACTUAL: #{f[:actual]}"
end
