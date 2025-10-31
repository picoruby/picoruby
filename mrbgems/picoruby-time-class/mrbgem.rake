# If you name this 'picoruby-time', VM code variable uint8_t time[]
# will conflict with time(2) function of <time.h>
MRuby::Gem::Specification.new('picoruby-time-class') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Time class'

  spec.add_dependency 'picoruby-env'
  spec.add_conflict 'mruby-time'

  spec.cc.defines << "_POSIX_TIMERS" # for clock_gettime()

  spec.require_name = 'time'
end

