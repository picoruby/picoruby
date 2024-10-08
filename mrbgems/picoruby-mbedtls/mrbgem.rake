MRuby::Gem::Specification.new('picoruby-mbedtls') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Mbed-TLS porting for PicoRuby'

  # Alternative to `mbedtls/include/mbedtls/mbedtls_config.h`
  #   You'll need to define flags according to your needs and
  #   it requires more RAM consumption.
  spec.cc.defines << "MBEDTLS_CONFIG_FILE='\"#{dir}/include/mbedtls_config.h\"'"

  MBEDTLS_VERSION = "v2.28.1"

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

  spec.cc.include_paths << "#{dir}/mbedtls/include"
  spec.objs += Dir.glob("#{dir}/mbedtls/library/*.{c,cpp,m,asm,S}").map { |f|
    f.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
  }
end

