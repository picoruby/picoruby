case RUBY_ENGINE
when "mruby/c"
  require 'picorubyvm'
  p PicoRubyVM.memory_statistics
when "mruby"
  p PicoRubyVM.memory_statistics
  p ObjectSpace.count_objects
end
