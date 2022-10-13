MRuby::Build.new do |conf|

  conf.toolchain

  conf.mrubyc

  conf.gembox "default"

  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"

end
