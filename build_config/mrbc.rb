MRuby::Build.new do |conf|

  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    conf.toolchain :visualcpp
  else
    conf.toolchain :gcc
  end

  conf.set_build_info

  conf.gem core: "mruby-compiler-prism"
  conf.gem core: "mruby-bin-mrbc-prism"

  conf.instance_variable_set :@mrbcfile, "bin/mrbc-prism"
  conf.disable_libmruby
end

