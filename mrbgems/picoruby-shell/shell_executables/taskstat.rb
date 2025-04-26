case RUBY_ENGINE
when "mruby"
  puts Task.stat
else
  puts "taskstat is not supproted on #{RUBY_ENGINE}"
end
