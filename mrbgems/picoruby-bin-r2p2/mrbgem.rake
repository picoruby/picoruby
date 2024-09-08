MRuby::Gem::Specification.new 'picoruby-bin-r2p2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'R2P2 executable for POSIX'
  spec.add_dependency 'picoruby-shell'

  spec.cc.defines << "MRBC_USE_HAL_POSIX"

  task "#{build_dir}/mrblib/r2p2.c" => "#{spec.dir}/mrblib/app.rb"

  exec = exefile("#{build.build_dir}/bin/r2p2")
  r2p2_objs = Dir.glob("#{spec.dir}/tools/r2p2/*.c").map do |f|
    objfile(f.pathmap("#{spec.build_dir}/tools/r2p2/%n"))
  end
  file exec => r2p2_objs << build.libmruby_static do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'r2p2'

end
