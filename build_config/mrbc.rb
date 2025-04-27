MRuby::Build.new do |conf|

  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    conf.toolchain :visualcpp
  else
    conf.toolchain :gcc
  end

  conf.picoruby(alloc_libc: true)

  conf.gem core: 'picoruby-mrubyc'
  conf.gem core: "mruby-bin-mrbc2"
  conf.gem core: "mruby-compiler2"

  conf.disable_presym
end

