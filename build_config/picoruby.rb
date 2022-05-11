#
# This build config works also in mruby/build_config
#

MRuby::Build.new do |conf|
  conf.toolchain

  disable_presym
  conf.mrbcfile = "#{conf.build_dir}/bin/picorbc"

  ENV['MRUBYC_BRANCH'] = "mrubyc3"
  ENV['MRUBYC_REVISION'] = "4e91963"
  conf.gem github: 'hasumikin/mruby-mrubyc', branch: 'master'
  conf.gem github: 'hasumikin/mruby-pico-compiler', branch: 'master'
  conf.gem github: 'hasumikin/mruby-bin-picorbc', branch: 'master'
  conf.gem github: 'hasumikin/mruby-bin-picoruby', branch: 'master'
  conf.gem github: 'hasumikin/mruby-bin-picoirb', branch: 'master'

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
end
