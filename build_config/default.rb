MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.gembox "picoruby"

  ENV['MRUBYC_BRANCH'] ||= "master"
  ENV['MRUBYC_REVISION'] ||= "5fab2b85dce8fc0780293235df6c0daa5fd57dce"
end

