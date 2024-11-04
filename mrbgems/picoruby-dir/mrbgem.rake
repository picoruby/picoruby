MRuby::Gem::Specification.new('picoruby-dir') do |spec|
  spec.license = 'MIT and MIT-like license'
  spec.authors = ['Internet Initiative Japan Inc.', 'Kevlin Henney', 'HASUMI Hitoshi']
  spec.summary = 'Dir class for PicoRuby (ported from mruby-dir)'

  spec.add_dependency 'picoruby-env'
  spec.add_conflict 'picoruby-filesystem-fat'
end

