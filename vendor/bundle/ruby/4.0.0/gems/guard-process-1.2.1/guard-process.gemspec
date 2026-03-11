# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require 'rbconfig' unless defined?(RbConfig)

Gem::Specification.new do |s|
  s.name        = "guard-process"
  s.version     = "1.2.1"
  s.authors     = ["Mark Kremer"]
  s.email       = ["mark@without-brains.net"]
  s.homepage    = ""
  s.summary     = %q{Guard extension to run cli processes}
  s.description = %q{Guard extension to run cli processes}

  s.required_ruby_version = '>= 1.9.3'

  s.files         = `git ls-files`.split("\n")
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]

  s.add_dependency('guard-compat', '~> 1.2', '>= 1.2.1')
  s.add_dependency('spoon', '~> 0.0.1')

  s.add_development_dependency('bundler')
end
