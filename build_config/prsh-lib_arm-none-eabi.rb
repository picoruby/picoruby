MRuby::CrossBuild.new("prsh_arm-none-eabi") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  #   arm-linux-gnueabihf | to make lemon and ptr_size_generator
  #   qemu-arm-static     | to run ptr_size_generator
  ###############################################################

  conf.toolchain
  conf.cc.flags << "-static"
  conf.linker.flags << "-static"
  conf.cc.command = "arm-none-eabi-gcc"
  conf.archiver.command = "arm-none-eabi-ar"
  # alt_command is for qemu execution on the host computer
  conf.cc.alt_command = "arm-linux-gnueabihf-gcc"
  conf.archiver.alt_command = "arm-linux-gnueabihf-ar"
  ENV['QEMU'] = "qemu-arm-static"

  conf.mrubyc_hal_arm
  conf.picoruby

  conf.gem core: "picoruby-filesystem-fat"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-terminal"
  conf.gem core: "picoruby-sandbox"

end


