MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  spec.add_dependency 'picoruby-time'
  spec.add_dependency 'picoruby-pack'
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'picoruby-jwt'
  spec.add_dependency 'picoruby-cyw43' unless build.posix?

  spec.add_conflict 'picoruby-socket'

  if build.posix?
    #
    # POSIX Build: Use standard UNIX network stack + mbedTLS
    # No LwIP required
    #
    puts "Building picoruby-net for POSIX (using UNIX network stack)"

    # Include mbedTLS headers
    spec.cc.include_paths << "#{File.expand_path('..', dir)}/picoruby-mbedtls/lib/mbedtls/include"

    # Clear default objs to prevent compiling src/*.c (LwIP-specific files)
    spec.objs.clear

    # Compile POSIX implementation files
    %w[dns tcp_client tls_client udp_client net].each do |name|
      src = "#{dir}/ports/posix/#{name}.c"
      if File.exist?(src)
        obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
        spec.objs << obj
      end
    end

  else
    #
    # Microcontroller Build: Use LwIP network stack
    #
    puts "Building picoruby-net for microcontroller (using LwIP)"

    LWIP_VERSION = "STABLE-2_2_1_RELEASE"
    LWIP_REPO = "https://github.com/lwip-tcpip/lwip"
    lwip_dir = "#{dir}/lib/lwip"

    # Clone or update LwIP repository
    if File.directory?(lwip_dir)
      # Check if it's a git repository and has the correct version
      if File.directory?("#{lwip_dir}/.git")
        current_branch = `cd #{lwip_dir} && git branch --show-current 2>/dev/null`.strip
        current_tag = `cd #{lwip_dir} && git describe --tags --exact-match HEAD 2>/dev/null`.strip

        # Check if we're on the wrong version
        unless current_branch == LWIP_VERSION || current_tag == LWIP_VERSION
          puts "lwip version mismatch. Fetching and checking out #{LWIP_VERSION}..."
          sh "cd #{lwip_dir} && git fetch origin #{LWIP_VERSION}:#{LWIP_VERSION} 2>/dev/null || git fetch origin"
          sh "cd #{lwip_dir} && git checkout #{LWIP_VERSION}"
        end
      else
        # Directory exists but not a git repo, remove and clone
        puts "lwip directory exists but is not a git repository. Removing and cloning..."
        FileUtils.rm_rf(lwip_dir)
        sh "git clone -b #{LWIP_VERSION} #{LWIP_REPO} #{lwip_dir}"
      end
    else
      # Directory doesn't exist, clone it
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

    spec.cc.include_paths << "#{lwip_dir}/src/include"
    spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
    spec.cc.include_paths << "#{lwip_dir}/src/apps/altcp_tls"
    spec.cc.include_paths << "#{File.expand_path('..', dir)}/picoruby-mbedtls/lib/mbedtls/include"

    # Compile LwIP source files
    Dir.glob("#{lwip_dir}/src/**/*.c").each do |src|
      next if src.end_with?('makefsdata.c')
      next if src.end_with?('altcp_tls_mbedtls.c')
      obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
      spec.objs << obj
    end
  end

  spec.posix
end
