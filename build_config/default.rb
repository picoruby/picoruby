MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "default"
  conf.gembox "r2p2"
  conf.gem core: "picoruby-bin-r2p2"

  ENV['MRUBYC_BRANCH'] ||= "master"
  ENV['MRUBYC_REVISION'] ||= "8fd092fc99d234e431a26e7bc9db3a3b56c0d681"
end

