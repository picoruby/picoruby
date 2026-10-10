# Test runner for `rake test:gems:picoruby32`: the host test build
# (picoruby-test.rb) compiled as a 32-bit executable with the mrb_int width
# and boxing of the 32-bit targets. Needs gcc-multilib. The picoruby binary
# is an i386 executable; some sandboxes refuse its system calls.
MRuby::CrossBuild.new('picoruby32-test') do |conf|
  conf.toolchain :gcc
  # A CrossBuild selects no gem port by itself
  conf.ports :posix

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "PICORB_DEBUG"
  # 32-bit word boxing: mrb_value is 4 bytes. An Integer past 31 bits is a
  # heap RInteger, and one past mrb_int is a Bignum from mruby-bigint.
  conf.cc.defines << "MRB_INT32"
  conf.cc.defines << "MRB_WORD_BOXING"
  conf.cc.defines << "MRB_32BIT"
  conf.cc.defines << "MRB_UTF8_STRING"

  # glibc on i386 keeps a 32-bit time_t unless asked; Time past 2038
  # would break down into the wrong calendar fields.
  conf.cc.defines << "_TIME_BITS=64"
  conf.cc.defines << "_FILE_OFFSET_BITS=64"

  conf.cc.flags << '-m32'
  conf.linker.flags << '-m32'

  conf.picoruby(alloc_align: 4)

  # No OpenSSL here: the socket gem's POSIX port links libssl, and a 32-bit
  # libssl is rarely installed. test.rake skips the socket-dependent gems
  # for this target.

  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-bigint"
  conf.gembox "mruby-posix"
  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gem core: 'picoruby-bin-picoruby'
  conf.gem core: 'picoruby-picotest'
end
