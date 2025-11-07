MRuby::Gem::Specification.new('picoruby-mbedtls') do |spec|
  spec.license = 'MIT'
  spec.authors = ['HASUMI Hitoshi', 'Ryo Kajiwara']
  spec.summary = 'Mbed-TLS porting for PicoRuby'

  spec.add_dependency 'picoruby-rng'
  spec.add_dependency 'picoruby-base64'

  MBEDTLS_VERSION = "v3.6.2"
  MBEDTLS_REPO = "https://github.com/Mbed-TLS/mbedtls.git"
  mbedtls_dir = "#{dir}/lib/mbedtls"

  if File.directory?(mbedtls_dir)
    # Check if it's a git repository and has the correct version
    if File.directory?("#{mbedtls_dir}/.git")
      current_tag = `cd #{mbedtls_dir} && git describe --tags --exact-match HEAD 2>/dev/null`.strip

      # Check if we're on the wrong version
      unless current_tag == MBEDTLS_VERSION
        puts "mbedtls version mismatch. Fetching and checking out #{MBEDTLS_VERSION}..."
        sh "cd #{mbedtls_dir} && git fetch origin #{MBEDTLS_VERSION}:#{MBEDTLS_VERSION} 2>/dev/null || git fetch origin"
        sh "cd #{mbedtls_dir} && git checkout #{MBEDTLS_VERSION}"
      end
    else
      # Directory exists but not a git repo, remove and clone
      puts "mbedtls directory exists but is not a git repository. Removing and cloning..."
      FileUtils.rm_rf(mbedtls_dir)
      sh "git clone -b #{MBEDTLS_VERSION} #{MBEDTLS_REPO} #{mbedtls_dir}"
    end
  else
    # Directory doesn't exist, clone it
    sh "git clone -b #{MBEDTLS_VERSION} #{MBEDTLS_REPO} #{mbedtls_dir}"
  end

  # Helper method to create symlinks in our include directory
  def create_symlink(link_path, target_path, base_dir = "#{dir}/include")
    full_link_path = File.join(base_dir, link_path)
    return if File.exist?(full_link_path)
    FileUtils.mkdir_p(File.dirname(full_link_path))
    FileUtils.ln_sf(target_path, full_link_path)
  end

  # Create symlinks for headers that lwip expects in our include directory
  symlinks = [
    # [link_path, target_path (relative to #{dir}/include)]
    ["mbedtls/certs.h", "../../lib/mbedtls/tests/include/test/certs.h"],
    ["mbedtls/ssl_internal.h", "../../lib/mbedtls/library/ssl_misc.h"],
    ["psa_util_internal.h", "../lib/mbedtls/library/psa_util_internal.h"],
    ["ssl_ciphersuites_internal.h", "../lib/mbedtls/library/ssl_ciphersuites_internal.h"],
    ["x509_internal.h", "../lib/mbedtls/library/x509_internal.h"],
    # Internal headers that lwip might need (conditionally added if they exist)
    *%w[
      pk_internal.h
      psa_crypto_core.h
      psa_crypto_invasive.h
      psa_crypto_its.h
      ssl_invasive.h
      ssl_debug_helpers_generated.h
      common.h
      alignment.h
    ].filter_map { |header|
      library_path = "#{mbedtls_dir}/library/#{header}"
      [header, "../lib/mbedtls/library/#{header}"] if File.exist?(library_path)
    }
  ]

  # Create all symlinks
  symlinks.each do |link_path, target_path|
    create_symlink(link_path, target_path)
  end

  spec.cc.defines << "MBEDTLS_CONFIG_FILE='\"#{dir}/include/mbedtls_config.h\"'"
  spec.cc.include_paths << "#{mbedtls_dir}/include"
  spec.cc.include_paths << "#{dir}/include"
  spec.objs += Dir.glob("#{mbedtls_dir}/library/*.{c,cpp,m,asm,S}").map do |f|
    f.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
  end

  if build.posix?
    cc.defines << "PICORB_PLATFORM_POSIX"
  end

  spec.posix
end

