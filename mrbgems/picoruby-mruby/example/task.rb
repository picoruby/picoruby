task1 = Task.new(name: "task1") do
  5.times do |i|
    sleep 1
    puts "#{i} #{Task.current.name}"
  end
end

task2 = Task.new(name: "task2") do
  7.times do |i|
    sleep 1
    puts "#{i} #{Task.current.name}"
  end
end

3.times do |i|
  puts "Hello from main task"
  sleep 1
end

puts Task.stat

task1.join

puts Task.stat

task2.join

puts Task.stat
