require 'json'

if RUBY_ENGINE == "mruby/c"
  require "metaprog"
  require 'dir'
elsif RUBY_ENGINE == "ruby"
  class Object
    def class?
      self.class == Class
    end
  end
  require_relative "./picotest/test"
  require_relative "./picotest/runner"
  require_relative "./workaround_dev"
end

module Picotest
  GREEN = "\e[32m"
  RED = "\e[31m"
  RESET = "\e[0m"
end
