MRuby::CrossBuild.new("prsh_arm-none-eabi") do |conf|

  conf.toolchain
  conf.cc.flags << "-static"
  conf.linker.flags << "-static"
  conf.cc.command = "arm-none-eabi-gcc"
  conf.archiver.command = "arm-none-eabi-ar"
  # alt_command is for qemu execution on the host computer
  conf.cc.alt_command = "arm-linux-gnueabihf-gcc"
  conf.archiver.alt_command = "arm-linux-gnueabihf-ar"
  ENV['QEMU'] = "qemu-arm-static"

  conf.picoruby

  conf.gem github: 'hasumikin/mruby-pico-compiler', branch: 'master'

  conf.gem core: 'picoruby-mrubyc'
  # RP2040 is the only target so far
  conf.cc.include_paths << "#{gems['picoruby-mrubyc'].clone.dir}/include/rp2040"

  conf.gem core: "picoruby-filesystem-fat"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-terminal"
  conf.gem core: "picoruby-sandbox"

end


