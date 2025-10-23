MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_ALLOC_ALIGN=8"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"

  conf.posix
  conf.microruby

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: 'picoruby-picotest'
  dir = "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems"
  conf.gem gemdir: "#{dir}/mruby-io"
  conf.gem gemdir: "#{dir}/mruby-toplevel-ext"
  conf.gem gemdir: "#{dir}/mruby-kernel-ext"
  conf.gem gemdir: "#{dir}/mruby-object-ext"
  conf.gem gemdir: "#{dir}/mruby-string-ext"
  conf.gem gemdir: "#{dir}/mruby-array-ext"
  conf.gem gemdir: "#{dir}/mruby-objectspace"
  conf.gem gemdir: "#{dir}/mruby-metaprog"
  conf.gem gemdir: "#{dir}/mruby-error"
  conf.gem gemdir: "#{dir}/mruby-sprintf"
end
