# If you name this 'picoruby-time', VM code variable uint8_t time[]
# will conflict with time(2) function of <time.h>
MRuby::Gem::Specification.new('picoruby-time-class') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Time class'

  spec.require_name = 'time'
end

