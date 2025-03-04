MRuby::Gem::Specification.new 'picoruby-bin-r2p2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'R2P2 executable for POSIX'
  spec.add_dependency 'picoruby-require'
  spec.add_dependency 'picoruby-shell'

  spec.cc.defines << "MRBC_USE_HAL_POSIX"

  app_src = "#{dir}/tools/r2p2/app.c"
  file app_src => "#{dir}/mrblib/app.rb" do |t|
    File.open(t.name, "w") do |f|
      mrbc.run f, t.source, "app", cdump: false
    end
  end

  exec = exefile("#{build.build_dir}/bin/r2p2")
  r2p2_src = "#{dir}/tools/r2p2/r2p2.c"
  r2p2_obj = objfile(r2p2_src.pathmap("#{build_dir}/tools/r2p2/%n"))
  task r2p2_obj => [app_src, r2p2_src] do |t|
    build.cc.run t.name, r2p2_src
  end
  file exec => [r2p2_obj,  build.libmruby_static] do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'r2p2'

end
