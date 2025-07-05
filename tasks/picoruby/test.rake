require 'tempfile'
require_relative "#{MRUBY_ROOT}/mrbgems/picoruby-picotest/mrblib/picotest.rb"

namespace :test do
  namespace :full do
    desc "Run full-build test for a gem on PicoRuby"
    task :picoruby, [:gem_name] do |t, args|
      gem_name = args[:gem_name]
      raise "gem_name is required" unless gem_name

      config_path = create_temp_build_config('picoruby-test.rb', gem_name)

      puts "Building test binary for #{gem_name} on PicoRuby..."
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake all"

      ENV['PICORUBY_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/picoruby")
      unless run_cruby_test_runner(gem_name)
        exit 1
      end
    ensure
      File.unlink(config_path) if config_path && File.exist?(config_path)
    end

    desc "Run full-build test for a gem on MicroRuby"
    task :microruby, [:gem_name] do |t, args|
      gem_name = args[:gem_name]
      raise "gem_name is required" unless gem_name

      config_path = create_temp_build_config('microruby-test.rb', gem_name)

      puts "Building test binary for #{gem_name} on MicroRuby..."
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake all"

      ENV['PICORUBY_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/microruby")
      unless run_cruby_test_runner(gem_name)
        exit 1
      end
    ensure
      File.unlink(config_path) if config_path && File.exist?(config_path)
    end
  end
end

def create_temp_build_config(base_config_name, gem_name)
  config_file = Tempfile.new(['test_config_', '.rb'])
  base_config_path = File.expand_path("../../../build_config/#{base_config_name}", __FILE__)
  config_content = File.read(base_config_path)

  injection_point = /conf\.(picoruby|microruby)/
  injection_text = "conf.gem core: '#{gem_name}'\n  conf.gem core: 'picoruby-picotest'\n  "
  config_content.sub!(injection_point, injection_text + '\1')

  config_file.write(config_content)
  config_file.close
  config_file.path
end

def run_cruby_test_runner(gem_name)
  gem_dir = File.expand_path("../../../mrbgems/#{gem_name}", __FILE__)

  unless gem_dir && Dir.exist?(gem_dir)
    puts "ERROR: Gem directory not found at `#{gem_dir}`"
    exit 1
  end

  gem_name = File.basename(gem_dir)
  lib_name = gem_name.gsub('picoruby-', '')
  test_dir = File.join(gem_dir, 'test')

  puts "Target VM: #{ENV['PICORUBY_TEST_TARGET_VM']}"
  puts "Test directory: #{test_dir}"
  puts "Library to require: #{lib_name}"

  # Set the RUBY environment variable so that Picotest::Runner's IO.popen
  # uses the correct VM binary (PicoRuby or MicroRuby).
  ENV['RUBY'] = ENV['PICORUBY_TEST_TARGET_VM']

  # Initialize and run the Picotest runner
  runner = Picotest::Runner.new(test_dir, nil, "/tmp", lib_name)
  runner.run
end
