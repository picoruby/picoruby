MRuby::Gem::Specification.new('picoruby-irq') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'IRQ module'

  spec.add_dependency 'picoruby-gpio'

  # The ISR-to-task event bridge needs Task::Queue and the scheduler
  # hook. Without mruby-task the gem still builds; only the bridge
  # (IRQ.bind / IRQ.unbind / IRQ.take) compiles out. picoruby-machine
  # owns the single hook slot and hands out scheduler services.
  if build.picoruby? && !build.platform?(:esp32)
    spec.add_dependency 'mruby-task'
    spec.add_dependency 'picoruby-machine'
  end

  spec.cc.include_paths << "#{dir}/include"
end
