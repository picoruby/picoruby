# ping - test network connectivity using DNS and HTTP

begin
  require 'net'
rescue LoadError
  puts "Network not available"
  return
end

unless ARGV.count == 1
  puts "Usage: ping hostname"
  return
end

host = ARGV[0]

puts "PING #{host}"

# Try DNS resolution
print "Resolving #{host}... "
begin
  # Try to make a simple connection to test reachability
  # Using port 80 (HTTP) as a common port
  client = Net::HTTPClient.new(host)
  response = client.get("/")

  if response
    puts "OK"
    puts "#{host} is reachable (HTTP status: #{response[:status]})"
  else
    puts "FAILED"
    puts "#{host} is not reachable"
  end
rescue => e
  puts "FAILED"
  puts "#{host} is not reachable: #{e.message}"
end
