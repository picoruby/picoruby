MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.gem github: 'picoruby/mruby-compiler2'
  conf.gem github: 'picoruby/mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: "picoruby-mruby"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"

  conf.gem core: "picoruby-json"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-string-ext"

#  conf.disable_presym

  conf.microruby
end
