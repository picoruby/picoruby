MRuby::Gem::Specification.new('picoruby-mbedtls') do |spec|
  spec.license = 'MIT'
  spec.authors = ['HASUMI Hitoshi', 'Ryo Kajiwara']
  spec.summary = 'Mbed-TLS porting for PicoRuby'

  spec.add_dependency 'picoruby-rng'

  MBEDTLS_VERSION = "v2.28.8"

  task :repo do
    clone = false
    if Dir.exist?("#{dir}/mbedtls")
      require 'open3'
      result, status = Open3.capture2("git describe --tags", chdir: "#{dir}/mbedtls")
      if !status.success?
        clone = true # This is likely going to fail anyway
      elsif result.strip != MBEDTLS_VERSION
        sh "git fetch --all", chdir: "#{dir}/mbedtls"
        sh "git checkout #{MBEDTLS_VERSION}", chdir: "#{dir}/mbedtls"
      end
    else
      clone = true
    end
    if clone
      sh "git clone -b #{MBEDTLS_VERSION} https://github.com/Mbed-TLS/mbedtls.git", chdir: dir
    end
  end
  Rake::Task[:repo].invoke

  spec.cc.defines << "MBEDTLS_CONFIG_FILE='\"#{dir}/include/mbedtls_config.h\"'"
  spec.cc.include_paths << "#{dir}/mbedtls/include"
  spec.objs += Dir.glob("#{dir}/mbedtls/library/*.{c,cpp,m,asm,S}").map do |f|
    f.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
  end

  if build.name == 'host'
    cc.defines << "PICORB_PLATFORM_POSIX"
  end
end

