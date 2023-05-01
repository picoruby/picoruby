MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "default"

  conf.cc.flags.delete "-O3"
  conf.cc.flags << "-O0"
  conf.cc.flags << "-g"
  conf.cc.flags << "-fno-inline"

  conf.cc.defines << "MRBC_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"
  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"

  conf.gem core: "picoruby-filesystem-fat"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-sqlite3"
  conf.gem core: "picoruby-bin-r2p2"

end

