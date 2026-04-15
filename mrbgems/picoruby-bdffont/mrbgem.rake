MRuby::Gem::Specification.new('picoruby-bdffont') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Shared BDF bitmap font rendering infrastructure'

  cc.include_paths << "#{dir}/include"
end
