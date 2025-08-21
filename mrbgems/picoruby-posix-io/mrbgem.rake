MRuby::Gem::Specification.new('picoruby-posix-io') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Internet Initiative Japan Inc.', 'mruby developers', 'HASUMI Hitoshi']
  spec.summary = 'IO and File class for PicoRuby in POSIX (ported from mruby-io)'

  spec.add_dependency 'picoruby-time-class'
  spec.add_dependency 'picoruby-metaprog'
  spec.add_conflict 'picoruby-filesystem-fat'
end
