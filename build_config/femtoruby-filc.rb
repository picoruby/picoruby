# Temporary build config for building the POSIX host (mruby/c VM) with Fil-C
# (https://fil-c.org/), a memory-safe C implementation.
# See picoruby-filc.rb for the mruby counterpart.
#
# Usage:
#   PICORB_DEBUG=1 MRUBY_CONFIG=femtoruby-filc rake
#
# The toolchain is picked up from FILC_ROOT (or the usual CC/CXX/LD).
# Fil-C needs its own OpenSSL in $FILC_ROOT/pizfix; build it with
# projects/openssl-3.5.7 from the fil-c source tree.
MRuby::Build.new do |conf|
  conf.toolchain :gcc

  filc_root = ENV['FILC_ROOT'] || "#{ENV['HOME']}/src/filc-0.681-linux-x86_64"
  unless ENV['CC']
    conf.cc.command = "#{filc_root}/build/bin/clang"
    conf.cxx.command = "#{filc_root}/build/bin/clang++"
    conf.linker.command = "#{filc_root}/build/bin/clang"
  end

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "PICORB_INT64"
  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "MRBC_USE_STRING_UTF8"

  # estalloc carves objects out of a plain `uint8_t heap_pool[]`. Fil-C treats
  # such a buffer as pointer-free storage, so every pointer written into it
  # loses its capability and panics on read back. Keep MRBC_ALLOC_LIBC but drop
  # the estalloc malloc/free overrides, so allocation goes to the libc
  # allocator, which is backed by the Fil-C GC.
  conf.femtoruby(alloc_estalloc: false)

  # Link OpenSSL libraries for socket SSL support
  conf.linker.libraries << 'ssl'
  conf.linker.libraries << 'crypto'

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gembox "networking"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"
end
