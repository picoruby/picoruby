MRuby::Gem::Specification.new('picoruby-socket') do |spec|
  spec.license = 'MIT'
  spec.author  = 'PicoRuby developers'
  spec.summary = 'CRuby-compatible Socket implementation for PicoRuby'
  spec.description = 'Provides TCPSocket, UDPSocket, and TCPServer classes compatible with CRuby'

  # Add include directory
  spec.cc.include_paths << "#{dir}/include"

  # Platform detection and conditional compilation
  spec.posix

  if build.posix?
    # POSIX platform
    spec.cc.defines << 'PICORB_PLATFORM_POSIX'
  end

  # Dependencies
  # None for Phase 1 (basic implementation)
end
