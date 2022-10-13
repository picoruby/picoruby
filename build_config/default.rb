MRuby::Build.new do |conf|

  ENV['MRUBYC_BRANCH'] = "mrubyc3.1"

  conf.toolchain

  conf.picoruby

  conf.gembox "default"

  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"

end
