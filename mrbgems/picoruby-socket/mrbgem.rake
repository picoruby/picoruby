MRuby::Gem::Specification.new('picoruby-socket') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'CRuby-compatible Socket implementation for PicoRuby'
  spec.description = 'Provides TCPSocket, UDPSocket, and TCPServer classes compatible with CRuby'

  spec.require_name = 'socket'

  # Dependencies
  unless build.posix?
    spec.add_dependency 'picoruby-mbedtls'  # SSL/TLS support for non-POSIX platforms
  end
  spec.add_conflict 'picoruby-net'

  # Add include directory
  spec.cc.include_paths << "#{dir}/include"

  # Add mbedtls include path for SSL support (non-POSIX only)
  unless build.posix? || build.name == "esp32"
    mbedtls_dir = "#{MRUBY_ROOT}/mrbgems/picoruby-mbedtls/lib/mbedtls"
    if File.directory?(mbedtls_dir)
      spec.cc.include_paths << "#{mbedtls_dir}/include"
    end
  end

  unless build.posix? || build.name == "esp32"
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

    # Apply patches to LwIP
    patch_file = "#{dir}/patches/lwip-altcp-proxyconnect.patch"
    if File.exist?(patch_file)
      proxyconnect_file = "#{lwip_dir}/src/apps/http/altcp_proxyconnect.c"
      if File.exist?(proxyconnect_file)
        patch_applied = `cd #{lwip_dir} && git apply --check #{patch_file} 2>&1`.strip
        if patch_applied.empty?
          sh "cd #{lwip_dir} && git apply #{patch_file}"
          puts "Applied patch: lwip-altcp-proxyconnect.patch"
        end
      end
    end

    spec.cc.defines << 'PICO_CYW43_ARCH_POLL=1'

    # Add LwIP include paths
    spec.cc.include_paths << "#{lwip_dir}/src/include"
    spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
    spec.cc.include_paths << "#{lwip_dir}/src/apps/altcp_tls"

    # Compile LwIP source files
    Dir.glob("#{lwip_dir}/src/**/*.c").each do |src|
      next if src.end_with?('makefsdata.c')
      next if src.end_with?('altcp_tls_mbedtls.c')  # Use custom version from ports/rp2040
      obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
      spec.objs << obj
    end
  end

  spec.posix
end
