MRuby::CrossBuild.new("r2p2-arm-bin") do |conf|

  conf.toolchain

  conf.cc.command = "arm-linux-gnueabihf-gcc"
  conf.cc.include_paths << "/usr/arm-linux-gnueabihf/include"
  conf.linker.command = "arm-linux-gnueabihf-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-linux-gnueabihf-ar"

  ENV['QEMU'] = "qemu-arm-static"

  conf.picoruby

  conf.gembox "default"

  conf.cc.defines << "MRBC_USE_HAL_POSIX"
  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"

  conf.gem core: "picoruby-filesystem-fat"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-bin-r2p2"

end

