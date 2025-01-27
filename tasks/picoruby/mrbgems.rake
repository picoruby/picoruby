# Workaround for picoruby-mrubyc
MRuby.each_target do |build|
  if build.gems['picoruby-mrubyc']
    build.libmruby_objs.flatten!.reject!{|o| o.include? "/mrbgems/gem_init.o"}
    Rake.application.tasks.select{|t| t.name.include? "mrbgems/gem_init.o"}.each(&:clear)
  end
end
