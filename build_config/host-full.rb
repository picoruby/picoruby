MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_NO_BOXING"
  conf.cc.defines << "MRB_UTF8_STRING"

  conf.picoruby

  # Link OpenSSL libraries for socket SSL support
  conf.linker.libraries << 'ssl'
  conf.linker.libraries << 'crypto'

  conf.gembox "mruby-posix"
  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gembox "networking"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"

  # The host build of R2P2 (default.rb) plus standard mruby gems that the
  # device gemboxes leave out, for CRuby libraries run on the host. Code
  # that works here can fail on a device, which does not have these gems.
  mruby_gems = "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems"
  %w[
    mruby-class-ext
    mruby-compar-ext
    mruby-range-ext
    mruby-symbol-ext
    mruby-struct
    mruby-set
    mruby-catch
    mruby-rational
    mruby-bigint
    mruby-complex
    mruby-coerce
    mruby-encoding-transcode
    mruby-errno
    mruby-exit
    mruby-warning
    mruby-random
    mruby-random-formatter
    mruby-random-urandom
    mruby-signal
    mruby-process
    mruby-proc-binding
    mruby-io-ftype
    mruby-objectspace-weakmap
    mruby-objectspace-finalizer
  ].each { |g| conf.gem gemdir: "#{mruby_gems}/#{g}" }
  # ":memory:" databases only: picoruby-vfs would take over File and Dir
  conf.gem core: "picoruby-sqlite3"
end
