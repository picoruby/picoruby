MRuby::Gem::Specification.new('picoruby-json') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'JSON parser for PicoRuby'

  # The parser and the generator are byte loops in Ruby: compiled with
  # picoruby-aot they run 4x faster on a pico2 (parse + generate).
  spec.aot = true
  spec.aot_include_widened = true
end
