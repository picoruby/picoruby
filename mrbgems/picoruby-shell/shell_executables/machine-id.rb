begin
  require 'machine'
  puts Machine.unique_id
rescue LoadError
  puts "machine is not available in this platform"
end
