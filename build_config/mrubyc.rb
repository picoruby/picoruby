#
# libmruby.a will include only mruby/c VM objects.
#
MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby(alloc_libc: true)

end
