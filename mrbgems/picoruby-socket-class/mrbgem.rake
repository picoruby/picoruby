MRuby::Gem::Specification.new('picoruby-socket-class') do |spec|
  spec.license = 'MIT'
  spec.author  = 'PicoRuby developers'
  spec.summary = 'CRuby-compatible Socket implementation for PicoRuby'
  spec.description = 'Provides TCPSocket, UDPSocket, and TCPServer classes compatible with CRuby'

  spec.require_name = 'socket'

  # Add include directory
  spec.cc.include_paths << "#{dir}/include"

  # Dependencies
  spec.add_dependency 'picoruby-mbedtls'  # Phase 5: SSL/TLS support

  # Add mbedtls include path for SSL support
  mbedtls_dir = "#{MRUBY_ROOT}/mrbgems/picoruby-mbedtls/lib/mbedtls"
  if File.directory?(mbedtls_dir)
    spec.cc.include_paths << "#{mbedtls_dir}/include"
  end

  # Platform detection and conditional compilation
  spec.posix

  if build.posix?
    # POSIX platform
    spec.cc.defines << 'PICORB_PLATFORM_POSIX'
  end
end
