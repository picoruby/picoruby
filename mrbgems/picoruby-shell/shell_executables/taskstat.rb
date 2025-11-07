case RUBY_ENGINE
when "mruby"
  stat = Task.stat
  puts "tick: #{stat[:tick]}, wakeup_tick: #{stat[:wakeup_tick]}"
  [:dormant, :ready, :waiting, :suspended].each do |key|
    puts "#{key}: #{stat[key].size} tasks"
    stat[key].each do |task|
      puts "  c_id: #{task[:c_id]}, c_ptr: #{task[:c_ptr].to_s(16)}, c_status: #{task[:c_status]}"
      puts "    ptr: #{task[:ptr].to_s(16)}, name: #{task[:name]}, priority: #{task[:priority]}"
      puts "    status: #{task[:status]}, reason: #{task[:reason]}"
      puts "    timeslice: #{task[:timeslice]},  wakeup_tick: #{task[:wakeup_tick]}"
    end
  end
else
  puts "taskstat is not supported on #{RUBY_ENGINE}"
end
