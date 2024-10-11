unless ENV['WIFI_MODULE'] == "cwy43"
  return
end

print "Waiting warm up... "
3.downto(0) do |i|
  print "#{i} "
  sleep 1
end
puts
system "wifi_connect --check-auto-connect"
