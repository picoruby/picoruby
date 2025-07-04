require 'tempfile'

namespace :test do
  namespace :full do
    desc "Run full-build test for a gem on PicoRuby"
    task :picoruby, [:gem_name] do |t, args|
      gem_name = args[:gem_name]
      raise "gem_name is required" unless gem_name

      puts "Preparing full-build test for #{gem_name} on PicoRuby..."

      # Create a temporary build config
      config_file = Tempfile.new(['test_config_', '.rb'])
      base_config_path = File.expand_path('../../build_config/default.rb', __dir__)
      config_content = File.read(base_config_path)
      # Inject the target gem into the config
      config_content.sub!(/conf\.picoruby\(/, "conf.gem core: '#{gem_name}'\n  conf.picoruby(")
      config_file.write(config_content)
      config_file.close

      # Build with the temporary config
      puts "Building with temporary config: #{config_file.path}"
      sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=#{config_file.path} rake all"

      # Run the test
      runner_binary = "./build/host/bin/picoruby"
      test_file = "mrbgems/#{gem_name}/test/test_#{gem_name.gsub('picoruby-', '')}.rb"
      run_test(runner_binary, test_file)
    ensure
      config_file.unlink if config_file
    end

    desc "Run full-build test for a gem on MicroRuby"
    task :microruby, [:gem_name] do |t, args|
      gem_name = args[:gem_name]
      raise "gem_name is required" unless gem_name

      puts "Preparing full-build test for #{gem_name} on MicroRuby..."

      # Create a temporary build config
      config_file = Tempfile.new(['test_config_', '.rb'])
      base_config_path = File.expand_path('../../build_config/microruby.rb', __dir__)
      config_content = File.read(base_config_path)
      # Inject the target gem into the config
      config_content.sub!(/conf\.microruby/, "conf.gem core: '#{gem_name}'\n  conf.microruby")
      config_file.write(config_content)
      config_file.close

      # Build with the temporary config
      puts "Building with temporary config: #{config_file.path}"
      sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=#{config_file.path} rake all"

      # Run the test
      runner_binary = "./build/host/bin/microruby"
      test_file = "mrbgems/#{gem_name}/test/test_#{gem_name.gsub('picoruby-', '')}.rb"
      run_test(runner_binary, test_file)
    ensure
      config_file.unlink if config_file
    end
  end
end

def run_test(runner_binary, test_file)
  raise "Test file not found: #{test_file}" unless File.exist?(test_file)

  cmd = "#{runner_binary} #{File.expand_path('tools/test_runner.rb')} #{File.expand_path(test_file)}"
  puts "Running test command: #{cmd}"
  system cmd
end