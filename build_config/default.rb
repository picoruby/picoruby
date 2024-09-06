MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "picoruby"
  conf.gembox "r2p2"
  conf.gem core: "picoruby-bin-r2p2"

  ENV['MRUBYC_BRANCH'] ||= "master"
  ENV['MRUBYC_REVISION'] ||= "52fcc668a3a175f6fd2c991d5c3b5f8eca927604"
end

