MRuby::Build.new do |conf|

  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    conf.toolchain :visualcpp
  else
    conf.toolchain :gcc
  end

  conf.cc.defines << "MRC_PARSER_PRISM"

  conf.picoruby

  conf.gem core: 'picoruby-mrubyc'
  conf.gem github: "picoruby/mruby-bin-mrbc2"
  conf.gem github: "picoruby/mruby-compiler2"

  conf.disable_presym
end

