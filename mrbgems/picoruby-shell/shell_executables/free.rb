case RUBY_ENGINE
when "mruby/c"
  require 'picorubyvm'
  p PicoRubyVM.memory_statistics
when "mruby"
  GC.disable
  p ObjectSpace.count_objects
  GC.enable
end
