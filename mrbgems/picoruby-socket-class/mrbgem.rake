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
  mbedtls_dir = "#{dir}/../picoruby-mbedtls/lib/mbedtls"
  if File.directory?(mbedtls_dir)
    spec.cc.include_paths << "#{mbedtls_dir}/include"
  end

  # Platform detection and conditional compilation
  spec.posix

  if build.posix?
    #
    # POSIX Build: Use standard UNIX sockets
    #
    puts "Building picoruby-socket-class for POSIX (using UNIX sockets)"
    spec.cc.defines << 'PICORB_PLATFORM_POSIX'

    # Add POSIX implementation files
    %w[tcp_socket udp_socket tcp_server ssl_socket].each do |name|
      src = "#{dir}/ports/posix/#{name}.c"
      if File.exist?(src)
        obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
        spec.objs << obj
      end
    end

  else
    #
    # Microcontroller Build (rp2040): Use LwIP network stack
    #
    puts "Building picoruby-socket-class for microcontroller (using LwIP)"

    # LwIP configuration
    LWIP_VERSION = "STABLE-2_2_1_RELEASE"
    LWIP_REPO = "https://github.com/lwip-tcpip/lwip"
    lwip_dir = "#{dir}/lib/lwip"

    # Clone or update LwIP repository
    if File.directory?(lwip_dir)
      if File.directory?("#{lwip_dir}/.git")
        current_branch = `cd #{lwip_dir} && git branch --show-current 2>/dev/null`.strip
        current_tag = `cd #{lwip_dir} && git describe --tags --exact-match HEAD 2>/dev/null`.strip

        unless current_branch == LWIP_VERSION || current_tag == LWIP_VERSION
          puts "lwip version mismatch. Fetching and checking out #{LWIP_VERSION}..."
          sh "cd #{lwip_dir} && git fetch origin #{LWIP_VERSION}:#{LWIP_VERSION} 2>/dev/null || git fetch origin"
          sh "cd #{lwip_dir} && git checkout #{LWIP_VERSION}"
        end
      else
        puts "lwip directory exists but is not a git repository. Removing and cloning..."
        FileUtils.rm_rf(lwip_dir)
        sh "git clone -b #{LWIP_VERSION} #{LWIP_REPO} #{lwip_dir}"
      end
    else
      sh "git clone -b #{LWIP_VERSION} #{LWIP_REPO} #{lwip_dir}"
    end

    # Add LwIP include paths
    spec.cc.include_paths << "#{lwip_dir}/src/include"
    spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
    spec.cc.include_paths << "#{lwip_dir}/src/apps/altcp_tls"

    # Add rp2040 implementation files
    %w[tcp_socket udp_socket tcp_server ssl_socket net_helpers].each do |name|
      src = "#{dir}/ports/rp2040/#{name}.c"
      if File.exist?(src)
        obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
        spec.objs << obj
      end
    end
  end
end
