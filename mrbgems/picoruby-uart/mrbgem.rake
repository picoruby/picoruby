MRuby::Gem::Specification.new('picoruby-uart') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'UART class / General peripherals'

  spec.add_dependency 'picoruby-gpio'
  spec.add_dependency 'picoruby-machine'

  # RX publishes to the ISR-to-task event bridge, except on ESP32, whose
  # producer is a FreeRTOS task that has never been brought onto it.
  # PICORB_IRQ_EVENT_BRIDGE does not express that: MRB_USE_TASK_SCHEDULER
  # is defined for every mruby build, so it is on for ESP32 too. The
  # matching guard is PICORB_UART_EVENT_BRIDGE in src/uart.c, and this
  # condition and that one have to stay identical -- disagree and either
  # IRQ_signal_from_isr fails to link or the bridge runs unannounced.
  spec.add_dependency 'picoruby-irq' unless build.platform?(:esp32)

  spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-machine/include"
  spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-irq/include"
end

