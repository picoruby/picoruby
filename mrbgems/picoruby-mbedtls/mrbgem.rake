MRuby::Gem::Specification.new('picoruby-mbedtls') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Mbed-TLS porting for PicoRuby'

  # Alternative to `mbedtls/include/mbedtls/mbedtls_config.h`
  #   You'll need to define flags according to your needs and
  #   it requires more RAM consumption.
  spec.cc.defines << "MBEDTLS_CONFIG_FILE='\"#{dir}/include/mbedtls_config.h\"'"

  spec.cc.include_paths << "#{dir}/mbedtls/include"
  spec.objs += Dir.glob("#{dir}/mbedtls/library/*.{c,cpp,m,asm,S}").map { |f|
    f.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
  }
#  spec.add_dependency 'picoruby-machine'
end

