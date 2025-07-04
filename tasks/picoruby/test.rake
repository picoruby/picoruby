namespace :test do
  desc "Run tests for PicoRuby (mruby/c VM)"
  task :picoruby do
    puts "Building test runner with default.rb..."
    config = File.expand_path('../../build_config/default.rb', __dir__)
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=#{config} rake all"

    puts "Starting PicoRuby test runner..."
    runner_binary = "./build/host/bin/picoruby"
    run_tests(runner_binary)
  end

  desc "Run tests for MicroRuby (mruby VM)"
  task :microruby do
    puts "Building test runner with microruby.rb..."
    config = File.expand_path('../../build_config/microruby.rb', __dir__)
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=#{config} rake all"

    puts "Starting MicroRuby test runner..."
    runner_binary = "./build/host/bin/microruby"
    run_tests(runner_binary)
  end
end

def run_tests(runner_binary)
  # Setup environment for the test runner
  load_paths = (Dir.glob('mrbgems/picoruby-*/mrblib') + Dir.glob('mrbgems/picoruby-*/test')).map do |path|
    File.expand_path(path)
  end.join(':')

  ENV['PICORUBY_LOAD_PATH'] = load_paths
  ENV['RUBY'] = File.expand_path(runner_binary)

  # The test runner script will be a separate file that uses Picotest::Runner
  # Use `system` to avoid aborting the rake task on failure, so we can see the output
  system("#{runner_binary} #{File.expand_path('tools/test_runner.rb')}")
end