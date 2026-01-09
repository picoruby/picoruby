MRuby::Gem::Specification.new('hal-picoruby-task') do |spec|
  spec.license = 'BSD-3-Clause'
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'HAL for mruby-task using picoruby HAL infrastructure'

  # Depend on mruby-task (but don't create circular dependency)
  # The actual HAL implementation is in picoruby-mruby/src/task_hal.c
end
