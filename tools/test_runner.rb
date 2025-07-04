# This runner executes a single test file.

# --- `require` logic ---
begin
  require 'picotest'
  require 'base64'
rescue LoadError
  # In PicoRuby, these are loaded by the test runner below
end

# --- Get test file path from argument ---
test_file_path = ARGV[0]
unless test_file_path && File.exist?(test_file_path)
  puts "ERROR: Test file not found at `#{test_file_path}`"
  raise "Stopping execution"
end

# --- Load the test file ---
puts "Loading test file: #{test_file_path}"
path_to_require = File.expand_path(test_file_path)
if path_to_require.end_with?('.rb')
  path_to_require = path_to_require[0..-4]
end
require path_to_require

# --- Dynamically find and run the test class ---
base = File.basename(test_file_path)
if base.end_with?('.rb')
  base = base[0..-4]
end
parts = base.split('_')
parts.shift
class_name_parts = parts.map do |part|
  part[0].upcase + part[1..-1]
end
class_name_str = "Test" + class_name_parts.join

test_class = Object.const_get(class_name_str)

puts "Running #{test_class}..."
test = test_class.new
test_methods = test.list_tests

test_methods.each do |method_name|
  puts; print "  #{test_class}##{method_name} "
  test.setup
  case method_name
  when :test_encode
    test.test_encode
  when :test_decode
    test.test_decode
  when :test_long_string
    test.test_long_string
  else
    puts "SKIPPED: Don't know how to run #{method_name}"
  end
  test.teardown
end
puts

# --- Summarize the result ---
result = test.result
puts
puts "Summary for #{test_class}:"
puts "  success: #{result['success_count']}, failure: #{result['failures'].size}, exception: #{result['exceptions'].size}"
result['failures'].each do |f|
  puts "  FAILURE: #{f[:method]}"
  puts "    Expected: #{f[:expected]}"
  puts "    Actual:   #{f[:actual]}"
end
result['exceptions'].each do |e|
  puts "  EXCEPTION: #{e[:method]}: #{e[:raise_message]}"
end

if result['failures'].size > 0 || result['exceptions'].size > 0
  raise "Test failed"
end