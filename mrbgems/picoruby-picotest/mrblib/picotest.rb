# Picotest entry point. Conditionally loads files based on RUBY_ENGINE:
# - CRuby ("ruby"): loads test.rb + runner.rb (orchestration side)
# - Target VM ("mruby"/"mruby/c"): test.rb + double.rb are loaded as pre-built gem

require 'json'

if RUBY_ENGINE == "mruby/c"
  require "posix-io"
  require "metaprog"
  require 'dir'
  require 'env'
elsif RUBY_ENGINE == "mruby"
  class Object
    def class?
      self.class == Class
    end
  end
elsif RUBY_ENGINE == "ruby"
  class Object
    def class?
      self.class == Class
    end
  end
  require_relative "./picotest/test"
  require_relative "./picotest/runner"
end

module Picotest
  GREEN = "\e[32m"
  RED = "\e[31m"
  RESET = "\e[0m"
end
