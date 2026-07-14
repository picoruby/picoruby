p PicoRubyVM.memory_statistics
if RUBY_ENGINE == "mruby"
  p ObjectSpace.count_objects
end
