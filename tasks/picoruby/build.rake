# The `mrbc`, `femtoruby` and `default`(picoruby) configs all use the default
# build name "host", so they share `build/host`. Switching between them (or
# between debug/prod of the same config) leaves stale objects behind and breaks
# the build. Record the last host build here and auto-clean only when the
# variant actually changes; repeated identical builds skip the clean and stay
# fast.
HOST_BUILD_MARKER = "#{MRUBY_ROOT}/build/.last_host_build".freeze

def build_host(config, debug:)
  id = "#{config}/#{debug ? 'debug' : 'prod'}"
  prev = (File.read(HOST_BUILD_MARKER).strip if File.exist?(HOST_BUILD_MARKER))
  if prev && prev != id
    prev_config = prev.split("/").first
    puts "==> host build changed (#{prev} -> #{id}); cleaning #{prev_config} first"
    sh "MRUBY_CONFIG=#{prev_config} rake clean"
  end
  env = "MRUBY_CONFIG=#{config}"
  env = "PICORB_DEBUG=1 #{env}" if debug
  sh "#{env} rake"
  mkdir_p File.dirname(HOST_BUILD_MARKER)
  File.write(HOST_BUILD_MARKER, id)
end

def clean_host(config)
  sh "MRUBY_CONFIG=#{config} rake clean"
  rm_f HOST_BUILD_MARKER
end

namespace :mrbc do
  desc "create mrbc debug build"
  task :debug do
    build_host("mrbc", debug: true)
  end

  desc "create mrbc production build"
  task :prod do
    build_host("mrbc", debug: false)
  end
end

namespace :femtoruby do
  desc "create production build"
  task :prod do
    build_host("femtoruby", debug: false)
  end

  desc "create debug build"
  task :debug do
    build_host("femtoruby", debug: true)
  end

  desc "clean build files"
  task :clean do
    clean_host("femtoruby")
  end
end

namespace :picoruby do
  desc "create production build"
  task :prod do
    build_host("default", debug: false)
  end

  desc "create debug build"
  task :debug do
    build_host("default", debug: true)
  end

  desc "clean build files"
  task :clean do
    clean_host("default")
  end
end
