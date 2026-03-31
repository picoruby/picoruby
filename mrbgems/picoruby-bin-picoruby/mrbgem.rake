MRuby::Gem::Specification.new('picoruby-bin-picoruby') do |spec|

  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = '(picoruby|femtoruby) command'

  if build.cxx_exception_enabled?
    build.compile_as_cxx("#{spec.dir}/tools/picoruby/picoruby.c")
  end

  spec.add_dependency('mruby-compiler2')

  if build.vm_mruby?
    BINNAME = 'picoruby'
    spec.add_dependency 'picoruby-mruby'
    spec.add_dependency 'mruby-io', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  elsif build.vm_mrubyc?
    BINNAME = 'femtoruby'
    spec.add_dependency 'picoruby-mrubyc'
  end

  build.bins << BINNAME

  picoruby_src = "#{dir}/tools/picoruby/picoruby.c"
  bin_obj = objfile(picoruby_src.pathmap("#{build_dir}/tools/picoruby/%n"))

  file bin_obj => ["#{MRUBY_ROOT}/include/picoruby.h", "#{dir}/tools/picoruby/picoruby.c"] do |f|
    Dir.glob("#{dir}/tools/picoruby/*.c").map do |f|
      cc.run objfile(f.pathmap("#{build_dir}/tools/picoruby/%n")), f
    end
  end

  file exefile("#{build.build_dir}/bin/#{BINNAME}") => [bin_obj, build.libmruby_static] do |f|
    build.linker.run f.name, f.prerequisites
  end

end
