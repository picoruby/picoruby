MRuby::Build.new do |conf|

  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    conf.toolchain :visualcpp
  else
    conf.toolchain :gcc
  end

  conf.set_build_info

  conf.gem core: "mruby-compiler"
  conf.gem core: "mruby-bin-mrbc"

  conf.instance_variable_set :@mrbcfile, "bin/picorbc"
  conf.disable_libmruby
end

