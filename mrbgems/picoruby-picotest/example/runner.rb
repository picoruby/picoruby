#
# Usage: picoruby path/to/runner.rb
#

require 'picotest'

dir = File.dirname(File.expand_path(__FILE__))
Picotest::Runner.run(dir)
