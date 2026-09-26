MRuby::Gem::Specification.new('picoruby-regexp') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Regular expressions on a Pike VM engine'

  spec.require_name = 'regexp'

  # both gems define Regexp and MatchData
  spec.add_conflict 'picoruby-regexp_light'

  # lib/regex_engine.c and regex_engine.h come from the upstream
  # repository through lib/import.rb. The engine needs only regex_malloc,
  # regex_realloc and regex_free, which src/regexp.c supplies over the
  # VM allocator.
  spec.cc.include_paths << "#{dir}/lib"

  engine = "#{dir}/lib/regex_engine.c"
  obj = "#{build_dir}/src/#{objfile('regex_engine')}"
  file obj => engine do |t|
    spec.cc.run t.name, t.prerequisites[0]
  end
  spec.objs << obj
end
