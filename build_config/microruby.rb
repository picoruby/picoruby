MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.gem github: 'picoruby/mruby-compiler2'
  conf.gem github: 'picoruby/mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: "picoruby-mruby"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  conf.gem core: "mruby-file-stat"

  conf.gem core: "picoruby-json"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-string-ext"

  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-time"
  conf.gem core: "picoruby-env"

  conf.gem core: "picoruby-io-console"
  conf.gem core: "picoruby-picorubyvm"
  conf.gem core: "picoruby-sandbox"

  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-objectspace"

  conf.gem core: "picoruby-shell"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-dir"

  conf.gem core: "picoruby-bin-r2p2"

#  conf.disable_presym

  conf.microruby
end
