require 'minitest/autorun'
require 'mocha/setup'

require 'guard/compat/test/helper'
require 'guard/process'

require 'pry' unless ENV['CI'] == 'true'

TEST_ROOT = File.expand_path(File.dirname(__FILE__))
