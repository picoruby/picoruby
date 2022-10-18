MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "default"

  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"

  conf.gem core: "picoruby-fatfs"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-bin-prsh"

end

