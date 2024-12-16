MRuby::Gem::Specification.new('picoruby-mrubyc') do |spec|

  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby/c library'

  lib_dir = build.mrubyc_lib_dir
  mrubyc_dir = "#{lib_dir}/mrubyc"
  mrubyc_src_dir = "#{mrubyc_dir}/src"
  mrblib_build_dir = "#{build_dir}/mrblib"

  if cc.defines.include?("PICORUBY_INT64")
    cc.defines << "MRBC_INT64"
  end

  Dir.glob("#{mrubyc_dir}/src/*.c").each do |mrubyc_src|
    next if mrubyc_src.end_with?("mrblib.c")
    obj = objfile(mrubyc_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => mrubyc_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end

  if cc.defines.include?("PICORUBY_PLATFORM=posix") || cc.defines.include?("MRBC_USE_HAL_POSIX")
    cc.include_paths << mrubyc_dir + "/hal/posix"
    hal_src = "#{mrubyc_dir}/hal/posix/hal.c"
    obj = objfile(hal_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => hal_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  else
    spec.add_dependency 'picoruby-machine'
    cc.include_paths << "#{build.gems['picoruby-machine'].dir}/include"
    cc.defines << "MRBC_USE_HAL=#{build.gems['picoruby-machine'].dir}/include"
  end

  directory mrblib_build_dir

  # Regenerate mrblib.c from mrblib/*.rb
  mrblib_rbs = %w[enum.rb array.rb global.rb hash.rb numeric.rb object.rb range.rb string.rb].map{|f|"#{mrubyc_dir}/mrblib/#{f}"}.freeze
  file "#{mrblib_build_dir}/mrblib.c" => [build.mrbcfile, mrblib_build_dir] + mrblib_rbs do |f|
    sh "#{build.mrbcfile} -B mrblib_bytecode -o #{f.name} #{mrblib_rbs.join(' ')}"
  end

  mrblib_c = "#{mrblib_build_dir}/mrblib.c"
  mrblib_o = objfile(mrblib_c.ext)

  file mrblib_o => mrblib_c do |f|
    cc.run f.name, f.prerequisites.first
  end

  objs << mrblib_o

  next if %w(clean deep_clean).include?(Rake.application.top_level_tasks.first)

  file mrubyc_dir do
    sh "git submodule update --init --recursive"
  end

end
