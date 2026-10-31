MRuby::Gem::Specification.new('picoruby-aot') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'AOT compilation of mrblib methods to mruby cfuncs, typed by Spinel'

  # Runtime helpers the generated C calls. The generator itself lives in
  # tools/ and runs on the host at build time.
  spec.cc.include_paths << "#{dir}/include"
  spec.export_include_paths << "#{dir}/include"
end
