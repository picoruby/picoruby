case RUBY_ENGINE
when "mruby"
  puts Task.stat
else
  puts "taskstat is not supported on #{RUBY_ENGINE}"
end
