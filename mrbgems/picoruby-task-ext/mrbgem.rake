MRuby::Gem::Specification.new('picoruby-task-ext') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Task class extension'

  spec.add_dependency 'picoruby-sandbox'

  spec.require_name = 'task'

  raise "Deprecated. Use mruby/c built-in Task class instead."
end

