MRuby::Build.new do |conf|
  conf.toolchain

  disable_presym
  conf.mrbcfile = "#{conf.build_dir}/bin/picorbc"

  ENV['MRUBYC_BRANCH'] = "mrubyc3"
  ENV['MRUBYC_REVISION'] = "4e91963"
  conf.gem core: 'mruby-mrubyc'
  conf.gem core: 'mruby-pico-compiler'
  conf.gem core: 'mruby-bin-picorbc'
  conf.gem core: 'mruby-bin-picoruby'
  conf.gem core: 'mruby-bin-picoirb'

  conf.cc.defines << "DISABLE_MRUBY"
  if ENV["PICORUBY_DEBUG_BUILD"]
    conf.cc.defines << "PICORUBY_DEBUG"
    conf.cc.flags.flatten!
    conf.cc.flags.reject! { |f| %w(-g -O3).include? f }
    conf.cc.flags << "-g3"
    conf.cc.flags << "-O0"
  else
    conf.cc.defines << "NDEBUG"
  end
  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"
  conf.cc.defines << "MRBC_USE_MATH"
end
