# Temporary build config for building the POSIX host (mruby VM) with Fil-C
# (https://fil-c.org/), a memory-safe C implementation.
# See femtoruby-filc.rb for the mruby/c counterpart.
#
# Usage:
#   PICORB_DEBUG=1 MRUBY_CONFIG=picoruby-filc rake
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

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_NO_BOXING"
  conf.cc.defines << "MRB_UTF8_STRING"

  # mrbconf.h enables MRB_USE_ETEXT_RO_DATA_P on Linux, which makes
  # mrb_ro_data_p() compare against the linker-provided etext/edata symbols.
  # Fil-C renames globals with a "pizlonated_" prefix, so those symbols do not
  # exist and the link fails. Falling back to the always-false mrb_ro_data_p()
  # only costs a copy of string literals.
  conf.cc.defines << "MRB_NO_DEFAULT_RO_DATA_P"

  # estalloc carves objects out of a plain `uint8_t vm_heap[]`. Fil-C treats
  # such a buffer as pointer-free storage, so every pointer written into it
  # loses its capability and panics on read back. Use the libc allocator, which
  # is backed by the Fil-C GC.
  conf.picoruby(alloc_estalloc: false)

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
end
