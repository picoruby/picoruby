MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "picoruby"

  ENV['MRUBYC_BRANCH'] ||= "master"
  ENV['MRUBYC_REVISION'] ||= "52fcc668a3a175f6fd2c991d5c3b5f8eca927604"
end

