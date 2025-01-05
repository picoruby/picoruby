MRuby::Gem::Specification.new('picoruby-bin-microruby') do |spec|

  BINNAME = 'microruby'

  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'microruby command'
  spec.bins = [BINNAME]
  spec.add_dependency('mruby-compiler2', :github => 'picoruby/mruby-compiler2')

  if build.cxx_exception_enabled?
    build.compile_as_cxx("#{spec.dir}/tools/microruby/microruby.c")
  end

  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/lib/prism/include"

  spec.add_dependency 'picoruby-mruby'
  spec.cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/include"

  microruby_src = "#{dir}/tools/microruby/microruby.c"
  microruby_obj = objfile(microruby_src.pathmap("#{build_dir}/tools/microruby/%n"))
  #picogem_init = File.expand_path("../picogem_init.c", build_dir)

  file microruby_obj => ["#{dir}/tools/microruby/microruby.c"] do |f|
    Dir.glob("#{dir}/tools/microruby/*.c").map do |f|
      cc.run objfile(f.pathmap("#{build_dir}/tools/microruby/%n")), f
    end
  end

  exec = exefile("#{build.build_dir}/bin/#{BINNAME}")

  mruby_objs = Rake::Task.tasks.select{ |t|
    t.name.match? /picoruby-mruby.+\.o\z/
  }.map(&:name)

  file exec => mruby_objs + [picoruby_obj] do |f|
    build.linker.run f.name, f.prerequisites
  end

  build.bins << BINNAME
end


