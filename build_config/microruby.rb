MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.gem github: 'picoruby/mruby-compiler2'
  conf.gem github: 'picoruby/mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: "picoruby-mruby"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-print"

#  conf.disable_presym

  conf.cc.include_paths << conf.gems['picoruby-mruby'].dir + '/lib/mruby/include'
  conf.cc.flags << '-O0'
  conf.cc.defines << "PICORB_VM_MRUBY"
end
