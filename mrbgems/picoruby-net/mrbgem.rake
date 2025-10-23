MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end
  spec.add_dependency 'picoruby-pack'
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'picoruby-jwt'
  unless spec.cc.defines.include? "PICORB_PLATFORM_POSIX"
    spec.add_dependency 'picoruby-cyw43'
  end

  LWIP_VERSION = "STABLE-2_2_1_RELEASE"
  LWIP_REPO = "https://github.com/lwip-tcpip/lwip"
  lwip_dir = "#{dir}/lib/lwip"

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

  spec.cc.defines << 'PICO_CYW43_ARCH_POLL=1'

  spec.cc.include_paths << "#{lwip_dir}/src/include"
  spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
  spec.cc.include_paths << "#{lwip_dir}/src/apps/altcp_tls"
  spec.cc.include_paths << "#{File.expand_path('..', dir)}/picoruby-mbedtls/lib/mbedtls/include"

  Dir.glob("#{lwip_dir}/src/**/*.c").each do |src|
    next if src.end_with?('makefsdata.c')
    next if src.end_with?('altcp_tls_mbedtls.c')
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end

  if build.posix?
    src = "#{lwip_dir}/contrib/ports/unix/port/sys_arch.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end

  spec.posix
end
