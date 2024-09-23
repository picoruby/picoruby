MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "default"
  conf.gembox "r2p2"
  conf.gem core: "picoruby-bin-r2p2"

  conf.linker.flags_after_libraries << '-lcrypto'
  conf.linker.flags_after_libraries << '-lssl'
end

