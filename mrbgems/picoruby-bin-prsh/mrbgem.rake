MRuby::Gem::Specification.new 'picoruby-bin-prsh' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'prsh executable'
  spec.add_dependency 'picoruby-fatfs', core: 'picoruby-fatfs'

  exec = exefile("#{build.build_dir}/bin/prsh")
  prsh_objs = Dir.glob("#{spec.dir}/tools/prsh/*.c").map do |f|
    objfile(f.pathmap("#{spec.build_dir}/tools/prsh/%n"))
  end
  file exec => prsh_objs << build.libmruby_static do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'prsh'
end
