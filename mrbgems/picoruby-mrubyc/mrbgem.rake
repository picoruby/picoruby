MRuby::Gem::Specification.new('picoruby-mrubyc') do |spec|

  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby/c library'

  lib_dir = ENV['MRUBYC_LIB_DIR']
  mrubyc_dir = "#{lib_dir}/mrubyc"
  mrubyc_src_dir = "#{mrubyc_dir}/src"
  mrblib_build_dir = "#{build_dir}/mrblib"

  file mrubyc_dir do
    branch = ENV['MRUBYC_BRANCH']
    revision = ENV['MRUBYC_REVISION']
    repo = ENV['MRUBYC_REPO'] || 'https://github.com/mrubyc/mrubyc.git'
    FileUtils.cd lib_dir do
      sh "git clone -b #{branch} #{repo}"
    end
    if revision
      FileUtils.cd mrubyc_dir do
        sh "git checkout #{revision}"
      end
    end
  end

  if Rake.application.top_level_tasks.first == "deep_clean"
    FileUtils.cd lib_dir do
      rm_rf "mrubyc"
    end
  else
    Rake::Task[mrubyc_dir].invoke
  end

  cc.include_paths.delete_if { |path| path.include?("no_impl") }
  cc.include_paths << cc.defines.find { |d|
    d.start_with? "MRBC_USE_HAL"
  }.then { |hal|
    if hal.nil?
      cc.defines << "MRBC_USE_HAL=#{MRUBY_ROOT}/include/hal_no_impl"
      "#{MRUBY_ROOT}/include/hal_no_impl"
    elsif hal.start_with?("MRBC_USE_HAL_")
      "#{mrubyc_dir}/#{hal.match(/\A(MRBC_USE_)(.+)\z/)[2].downcase.sub("_", "/")}"
    else
      "#{mrubyc_dir}/#{hal.match(/\A(MRBC_USE_HAL=)(.+)\z/)[2]}"
    end
  }

  # Reject src/mrblib.c because it is possibly old
  mrubyc_srcs = Dir.glob("#{mrubyc_dir}/src/*.c").reject{|s|s.end_with?("mrblib.c")}.freeze
  # So, we regenerate mrblib.c from mrblib/*.rb
  mrblib_rbs = %w[enum.rb array.rb global.rb hash.rb numeric.rb object.rb range.rb string.rb].map{|f|"#{mrubyc_dir}/mrblib/#{f}"}.freeze

  mrubyc_srcs.each do |mrubyc_src|
    obj = objfile(mrubyc_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => mrubyc_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end
  if cc.defines.include?("MRBC_USE_HAL_POSIX")
    hal_src = "#{mrubyc_dir}/hal/posix/hal.c"
    obj = objfile(hal_src.pathmap("#{build_dir}/src/%n"))
    build.libmruby_objs << obj
    file obj => hal_src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end

  directory mrblib_build_dir

  file "#{mrblib_build_dir}/mrblib.c" => [build.mrbcfile, mrblib_build_dir] + mrblib_rbs do |f|
    sh "#{build.mrbcfile} -B mrblib_bytecode -o #{f.name} #{mrblib_rbs.join(' ')}"
  end

  mrblib_c = "#{mrblib_build_dir}/mrblib.c"
  mrblib_o = objfile(mrblib_c.ext)

  file mrblib_o => mrblib_c do |f|
    cc.run f.name, f.prerequisites.first
  end

  objs << mrblib_o
end
