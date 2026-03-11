# -*- encoding: utf-8 -*-
# stub: guard-process 1.2.1 ruby lib

Gem::Specification.new do |s|
  s.name = "guard-process".freeze
  s.version = "1.2.1".freeze

  s.required_rubygems_version = Gem::Requirement.new(">= 0".freeze) if s.respond_to? :required_rubygems_version=
  s.require_paths = ["lib".freeze]
  s.authors = ["Mark Kremer".freeze]
  s.date = "2015-01-16"
  s.description = "Guard extension to run cli processes".freeze
  s.email = ["mark@without-brains.net".freeze]
  s.homepage = "".freeze
  s.required_ruby_version = Gem::Requirement.new(">= 1.9.3".freeze)
  s.rubygems_version = "2.4.3".freeze
  s.summary = "Guard extension to run cli processes".freeze

  s.installed_by_version = "4.0.3".freeze

  s.specification_version = 4

  s.add_runtime_dependency(%q<guard-compat>.freeze, ["~> 1.2".freeze, ">= 1.2.1".freeze])
  s.add_runtime_dependency(%q<spoon>.freeze, ["~> 0.0.1".freeze])
  s.add_development_dependency(%q<bundler>.freeze, [">= 0".freeze])
end
