MRuby::Gem::Specification.new('picoruby-bin-picoruby2') do |spec|

  BINNAME = 'picoruby'

  spec.license = 'MIT'
  spec.author  = 'picoruby developers'
  spec.summary = 'picoruby command'
  spec.bins = [BINNAME]
  spec.add_dependency('mruby-compiler2', :github => 'picoruby/mruby-compiler2')
  spec.add_conflict 'picoruby-bin-picoruby'

  if build.cxx_exception_enabled?
    build.compile_as_cxx("#{spec.dir}/tools/picoruby/picoruby.c")
  end

  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/lib/prism/include"

  spec.add_dependency 'picoruby-mrubyc'
  spec.cc.include_paths << "#{build.gems['picoruby-mrubyc'].dir}/repos/mrubyc/src"

  picoruby_src = "#{dir}/tools/picoruby/picoruby.c"
  picoruby_obj = objfile(picoruby_src.pathmap("#{build_dir}/tools/picoruby/%n"))

  file picoruby_obj => "#{dir}/tools/picoruby/picoruby.c" do |f|
    Dir.glob("#{dir}/tools/picoruby/*.c").map do |f|
      cc.run objfile(f.pathmap("#{build_dir}/tools/picoruby/%n")), f
    end
  end

  exec = exefile("#{build.build_dir}/bin/#{BINNAME}")

  pico_compiler_objs = []
#  pico_compiler_objs = Rake::Task.tasks.select{ |t|
#    t.name.match? /mruby-pico-compiler.+\.o\z/
#  }.map(&:name).reject { |o| o.end_with? "parse.o" }

  mrubyc_objs = Rake::Task.tasks.select{ |t|
    t.name.match? /picoruby-mrubyc.+\.o\z/
  }.map(&:name)

  file exec => pico_compiler_objs + mrubyc_objs + [picoruby_obj] do |f|
    build.linker.run f.name, f.prerequisites
  end

  build.bins << BINNAME
end

