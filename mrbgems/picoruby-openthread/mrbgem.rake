MRuby::Gem::Specification.new('picoruby-openthread') do |spec|
  spec.license = 'MIT'
  spec.author  = 'ryosk7'
  spec.summary = 'OpenThread (IEEE 802.15.4 / Thread) networking for PicoRuby'

  # Machine.board_millis for the attach timeout, and the scheduler-service
  # multiplexer that drives OpenThread's tasklet loop.
  spec.add_dependency 'picoruby-machine'
  # Task::Queue carries role changes to a waiting wait_attached. mruby/c
  # has it built in; mruby gets it from mruby-task.
  spec.add_dependency 'mruby-task' if build.picoruby?

  spec.cc.include_paths << "#{dir}/include"

  # Deliberately absent: picoruby-socket (no lwIP on this part), and any
  # OpenThread include path. Only ports/nrf52/openthread.c sees the stack,
  # and that file is compiled by the product Makefile, which is the only
  # place that knows where the OpenThread tree lives.
end
