Dir.open("./") do |dir|
  while entry = dir.read
    puts entry
  end
end
