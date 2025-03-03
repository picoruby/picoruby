MRuby::Gem::Specification.new('picoruby-bin-microruby') do |spec|

  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = '(microruby|picoruby) command'

  if build.cxx_exception_enabled?
    build.compile_as_cxx("#{spec.dir}/tools/microruby/microruby.c")
  end

  spec.add_dependency('mruby-compiler2', :github => 'picoruby/mruby-compiler2')
  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/lib/prism/include"

  if build.vm_mruby?
    BINNAME = 'microruby'
    spec.add_dependency 'picoruby-mruby'
    spec.cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/lib/mruby/include"
  elsif build.vm_mrubyc?
    BINNAME = 'picoruby'
    spec.add_dependency 'picoruby-mrubyc'
    spec.cc.include_paths << "#{build_dir}/.."
  end

  build.bins << BINNAME

  microruby_src = "#{dir}/tools/microruby/microruby.c"
  bin_obj = objfile(microruby_src.pathmap("#{build_dir}/tools/microruby/%n"))

  file bin_obj => ["#{MRUBY_ROOT}/include/picoruby.h", "#{dir}/tools/microruby/microruby.c"] do |f|
    Dir.glob("#{dir}/tools/microruby/*.c").map do |f|
      cc.run objfile(f.pathmap("#{build_dir}/tools/microruby/%n")), f
    end
  end

  file exefile("#{build.build_dir}/bin/#{BINNAME}") => [bin_obj, build.libmruby_static] do |f|
    build.linker.run f.name, f.prerequisites
  end

end
