MRuby::Build.new do |conf|

  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    conf.toolchain :visualcpp
  else
    conf.toolchain :gcc
  end

  conf.gem core: "mruby-compiler2"
  conf.gem core: "mruby-bin-mrbc2"

  conf.instance_variable_set :@mrbcfile, "bin/picorbc"
  conf.disable_libmruby
  conf.disable_presym
end

