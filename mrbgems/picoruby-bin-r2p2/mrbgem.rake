MRuby::Gem::Specification.new 'picoruby-bin-r2p2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'R2P2 executable for POSIX'
  spec.add_dependency 'picoruby-shell'

  spec.cc.defines << "MRBC_USE_HAL_POSIX"
  hal_src = "#{build.gems['picoruby-mrubyc'].dir}/repos/mrubyc/src/hal_posix/hal.c"
  hal_obj = objfile(hal_src.pathmap("#{build.gems['picoruby-mrubyc'].build_dir}/src/%n"))
  file hal_obj => hal_src do |f|
    cc.run f.name, f.prerequisites.first
  end

  task "#{build_dir}/mrblib/r2p2.c" => "#{spec.dir}/mrblib/app.rb"

  exec = exefile("#{build.build_dir}/bin/r2p2")
  r2p2_objs = Dir.glob("#{spec.dir}/tools/r2p2/*.c").map do |f|
    objfile(f.pathmap("#{spec.build_dir}/tools/r2p2/%n"))
  end
  file exec => r2p2_objs << build.libmruby_static << hal_obj do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'r2p2'

end
