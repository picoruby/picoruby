1.upto(1000000) do |i|
  sleep 0.5
  if i % 3 == 0 || i.to_s.include?("3")
    $aho = true
  else
    puts i
  end
end

