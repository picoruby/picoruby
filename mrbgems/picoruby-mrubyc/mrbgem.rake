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

  next if %w(clean deep_clean).include?(Rake.application.top_level_tasks.first)

  # Run autogen at rake load time so _autogen_*.h files exist before any
  # compilation tasks run (including gems that include mrubyc headers).
  FileUtils.cd "#{mrubyc_dir}/src" do
    sh "make autogen"
  end

  Dir.glob("#{mrubyc_dir}/src/*.c").each do |mrubyc_src|
    obj = objfile(mrubyc_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => mrubyc_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end

end
