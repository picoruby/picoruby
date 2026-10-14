MRuby::Gem::Specification.new('picoruby-aot-probe') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Typed methods that probe the integer semantics of picoruby-aot (tests only)'

  # The twin compiles the probe's defs with picoruby-aot and runs the same
  # tests against the irep. The gem is not for a device build.
  spec.add_dependency 'picoruby-mruby'
  spec.aot = true
  spec.aot_include_widened = true
end
