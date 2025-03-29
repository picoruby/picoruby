MRuby::Gem::Specification.new('picoruby-mrubyc') do |spec|

  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby/c library'

  spec.add_dependency 'picoruby-machine'

  mrubyc_dir = "#{dir}/lib/mrubyc"

  file mrubyc_dir do
    sh "git submodule update --init --recursive"
  end

  file "#{mrubyc_dir}/src/mrblib.c" => build.mrbcfile do
    FileUtils.cd "#{mrubyc_dir}/mrblib" do
      sh "make MRBC=#{build.mrbcfile}"
    end
  end

  Dir.glob("#{mrubyc_dir}/src/*.c").each do |mrubyc_src|
    obj = objfile(mrubyc_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => mrubyc_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end

  if cc.build.posix?
    hal_src = "#{mrubyc_dir}/hal/posix/hal.c"
    obj = objfile(hal_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => hal_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end

  next if %w(clean deep_clean).include?(Rake.application.top_level_tasks.first)

end
