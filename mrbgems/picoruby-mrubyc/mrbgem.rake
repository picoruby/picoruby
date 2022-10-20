MRuby::Gem::Specification.new('picoruby-mrubyc') do |spec|
  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby/c library'

  mrubyc_dir = "#{dir}/repos/mrubyc"

  file mrubyc_dir do
    branch = ENV['MRUBYC_BRANCH'] || "master"
    revision = ENV['MRUBYC_REVISION']
    FileUtils.cd "#{dir}/repos" do
      sh "git clone -b #{branch} https://github.com/mrubyc/mrubyc.git"
    end
    if revision
      FileUtils.cd "#{dir}/repos/mrubyc" do
        sh "git checkout #{revision}"
      end
    end
  end

  mrubyc_srcs = %w(alloc   c_math    c_range  console keyvalue rrt0    vm
                   c_array c_numeric c_string error   load     symbol
                   c_hash  c_object  class    global  value)

  hal_dir = cc.defines.find { |d|
    d.start_with? "MRBC_USE_HAL"
  }.then { |hal|
    if hal.nil?
      # skip
    elsif hal.start_with?("MRBC_USE_HAL_")
      hal.match(/\A(MRBC_USE_)(.+)\z/)[2].downcase
    else
      hal.match(/\A(MRBC_USE_HAL=)(.+)\z/)[2]
    end
  }
  if hal_dir
    mrubyc_srcs << "#{hal_dir}/hal"
    cc.include_paths << hal_dir
  end

  if cc.defines.include?("DISABLE_MRUBY")
    build.libmruby_objs.clear
  end
  (mrubyc_srcs << "mrblib").each do |mrubyc_src|
    obj = objfile("#{build_dir}/src/#{mrubyc_src}")
    build.libmruby_objs << obj
    file obj => "#{mrubyc_dir}/src/#{mrubyc_src}.c" do |f|
      cc.run f.name, f.prerequisites.first
    end
    file "#{mrubyc_dir}/src/#{mrubyc_src}.c" => mrubyc_dir
  end
  file "#{mrubyc_dir}/mrblib" => mrubyc_dir

  file "#{build_dir}/src/mrblib.c" => [build.mrbcfile, "#{mrubyc_dir}/mrblib"] do |f|
    mrblib_sources = Dir.glob("#{mrubyc_dir}/mrblib/*.rb").join(" ")
    mkdir_p File.dirname(f.name)
    sh "#{build.mrbcfile} -B mrblib_bytecode -o #{f.name} #{mrblib_sources}"
  end

  file objfile("#{build_dir}/src/mrblib") => "#{build_dir}/src/mrblib.c" do |f|
    cc.run f.name, f.prerequisites.first
  end

end
