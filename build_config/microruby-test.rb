MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_ALLOC_ALIGN=8"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"

  conf.gem core: 'mruby-compiler2'
  conf.gem core: 'mruby-bin-mrbc2'
  conf.gem core: 'picoruby-mruby'
  conf.gem core: 'picoruby-machine'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: 'picoruby-require'
  conf.gem core: "picoruby-picorubyvm"
  conf.gem core: 'picoruby-picotest'
  dir = "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems"
  conf.gem gemdir: "#{dir}/mruby-kernel-ext"
  conf.gem gemdir: "#{dir}/mruby-string-ext"
  conf.gem gemdir: "#{dir}/mruby-array-ext"
  conf.gem gemdir: "#{dir}/mruby-time"
  conf.gem gemdir: "#{dir}/mruby-objectspace"
  conf.gem gemdir: "#{dir}/mruby-metaprog"
  conf.gem gemdir: "#{dir}/mruby-error"
  conf.gem gemdir: "#{dir}/mruby-sprintf"
  conf.gem gemdir: "#{dir}/mruby-dir"
  conf.gem gemdir: "#{dir}/mruby-io"

  conf.microruby
end

