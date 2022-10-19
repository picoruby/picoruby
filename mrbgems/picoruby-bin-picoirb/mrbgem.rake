MRuby::Gem::Specification.new 'picoruby-bin-picoirb' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'picoirb executable'
  spec.add_dependency 'mruby-pico-compiler', github: 'hasumikin/mruby-pico-compiler'
  spec.add_dependency 'picoruby-mrubyc'

  spec.cc.include_paths << "#{build.gems['picoruby-mrubyc'].dir}/repos/mrubyc/src"

  picoirb_mrblib_rbs = Dir.glob("#{dir}/tools/picoirb/*.rb")
  picoirb_mrblib_srcs = picoirb_mrblib_rbs.map do |rb|
    rb.pathmap("%X.c")
  end
  picoirb_mrblib_srcs.each do |src|
    file src => src.pathmap("%X.rb") do |f|
      sh "#{build.mrbcfile} -B #{f.name.pathmap("%n")} -o #{f.name} #{f.prerequisites.first}"
    end
  end

  picoirb_srcs = %w(picoirb sandbox).map{ |s| "#{dir}/tools/picoirb/#{s}.c" }
  picoirb_objs = picoirb_srcs.map do |picoirb_src|
    objfile(picoirb_src.pathmap("#{build_dir}/tools/picoirb/%n"))
  end
  picoirb_objs.each_with_index do |picoirb_obj, index|
    file picoirb_srcs[index] => picoirb_mrblib_srcs
    file picoirb_obj => picoirb_srcs[index] do |f|
      cc.run f.name, picoirb_srcs[index]
    end
  end

  exec = exefile("#{build.build_dir}/bin/picoirb")

  pico_compiler_objs = Rake::Task.tasks.select{ |t|
    t.name.match? /mruby-pico-compiler.+\.o\z/
  }.map(&:name).reject { |o| o.end_with? "parse.o" }

  mrubyc_objs = Rake::Task.tasks.select{
    |t| t.name.match? /picoruby-mrubyc.+\.o\z/
  }.map(&:name)

  file exec => pico_compiler_objs + mrubyc_objs + picoirb_objs do |f|
    build.linker.run f.name, f.prerequisites
  end

  build.bins << 'picoirb'
end
