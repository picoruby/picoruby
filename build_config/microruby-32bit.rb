MRuby::CrossBuild.new('microruby-32bit') do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  #conf.cc.defines << "PICORB_ALLOC_TLSF"
  #conf.cc.defines << "POOL_ALIGNMENT=8"
  #conf.cc.defines << "PICORB_ALLOC_TINYALLOC"
  conf.cc.defines << "PICORB_ALLOC_O1HEAP"
  n = 13 # Chenge this value to adjust the heap size
  ptr_size = 4 # 32-bit
  rvalue_words = 6 # word boxing
  rvalue_size = ptr_size * rvalue_words
  mrb_heap_header_size = ptr_size * 4 # see mruby/src/gc.c
  o1heap_header_size = ptr_size * 4 # see o1heap.c
  optimal_heap_page_size = 2 ** n # This should be power of 2 or just little bit smaller
  heap_page_size = (optimal_heap_page_size - mrb_heap_header_size - o1heap_header_size) / rvalue_size
  conf.cc.defines << "MRB_HEAP_PAGE_SIZE=#{heap_page_size}"

  conf.cc.flags << '-m32'
  conf.cc.flags << '-Wall'
  conf.cc.flags << '-Wextra'
  conf.cc.flags << '-Werror=address-of-packed-member'
  conf.cc.flags << '-Wno-unused-parameter'
  conf.cc.flags << '-mno-stackrealign'
  conf.cc.flags << '-falign-functions=2'
  conf.cc.flags << '-falign-jumps=2'
  conf.cc.flags << '-falign-loops=2'
  conf.cc.flags << '-falign-labels=2'
  conf.linker.flags << '-m32'

  conf.gem github: 'picoruby/mruby-compiler2'
  conf.gem github: 'picoruby/mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: "picoruby-mruby"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  conf.gem core: "picoruby-machine"
  conf.gem core: "mruby-file-stat"

  conf.gem core: "picoruby-json"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-string-ext"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-array-ext"

  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-time"
  conf.gem core: "picoruby-env"

  conf.gem core: "picoruby-io-console"
  conf.gem core: "picoruby-picorubyvm"
  conf.gem core: "picoruby-sandbox"

  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-objectspace"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-metaprog"

  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-picoline"
  conf.gem core: "picoruby-yaml"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-dir"

  conf.gem core: "picoruby-bin-r2p2"

  conf.microruby
end

