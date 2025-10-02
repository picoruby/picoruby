require 'tempfile'
require_relative "#{MRUBY_ROOT}/mrbgems/picoruby-picotest/mrblib/picotest.rb"

desc "shorthand for test:gems:steep"
task :steep => "test:gems:steep"

desc "shorthand for test:all"
task :test => "test:all"

namespace :test do

  desc "run all tests"
  task :all => ["compiler:picoruby", "compiler:microruby", "gems:steep", "gems:microruby", "gems:picoruby"]

  task :build_picoruby_test do
    puts "Building test runner with picoruby-test.rb..."
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=picoruby-test rake clean"
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=picoruby-test rake --silent all"
  end

  task :build_microruby_test do
    puts "Building test runner with microruby-test.rb..."
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=microruby-test rake clean"
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=microruby-test rake --silent all"
  end

  namespace :compiler do
    desc "run compiler tests with mruby VM"
    task :microruby => :build_microruby_test do
      ENV['USE_MRUBY'] = "yes"
      ENV['PICORBC_COMMAND'] = picorbcfile
      ENV['PICORUBY_COMMAND'] = microrubyfile
      sh "mrbgems/mruby-compiler2/compiler_test/helper/test.rb"
    end

    desc "run compiler tests with mruby/c VM"
    task :picoruby => :build_picoruby_test do
      ENV['PICORUBY_COMMAND'] = picorubyfile
      sh "mrbgems/mruby-compiler2/compiler_test/helper/test.rb"
      ENV['PICORUBY_COMMAND'] = nil
    end
  end

  namespace :gems do
    desc "steep check"
    task :steep do
      sh "bundle exec steep check"
    end

    desc "run test for a gem on PicoRuby"
    task :picoruby, [:specified_gem] do |t, args|
      unless run_test_for_gems('picoruby', args[:specified_gem])
        exit 1
      end
    end

    desc "run test for a gem on MicroRuby"
    task :microruby, [:specified_gem] do |t, args|
      unless run_test_for_gems('microruby', args[:specified_gem])
        exit 1
      end
    end

  end
end

def run_test_for_gems(vm_type, specified_gem)
  all_success = true
  gems = collect_gems(vm_type, specified_gem)
  if gems[:need_build].empty? && gems[:no_need_build].empty?
    puts "No gems found for testing."
    return false
  end
  ENV['PICORUBY_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/#{vm_type}")
  gems[:need_build].each do |gem_name|
    puts "Strategy: Full build for '#{gem_name}'"
    config_path = create_temp_build_config("#{vm_type}-test.rb", gem_name)
    lib_name = gem_name.gsub('picoruby-', '')
    begin
      puts "Building test binary for #{gem_name} on #{vm_type}..."
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake --silent all"
      unless run_picotest_runner(gem_name, lib_name, [])
        all_success = false
      end
    ensure
      File.unlink(config_path) if config_path && File.exist?(config_path)
    end
  end
  # clean build test runner for no_need_build just once
  unless gems[:no_need_build].empty?
    Rake::Task["test:build_#{vm_type}_test"].invoke
  end
  gems[:no_need_build].each do |gem_name|
    gem_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/#{gem_name}")
    unless Dir.exist?("#{gem_dir}/mrblib")
      puts "No mrblib directory found for gem '#{gem_name}'."
      puts "Skipping tests for this gem."
      next
    end
    load_files = []
    FileUtils.cd(gem_dir) do
      load_files += Dir.glob("mrblib/**/*.rb")
    end
    unless run_picotest_runner(gem_name, nil, load_files)
      all_success = false
    end
  end
  unless all_success
    print Picotest::RED
    puts "Some tests failed. Please check the output above."
    print Picotest::RESET
  end
  return all_success
end

def collect_gems(vm_type, specified_gem = nil)
  vm = vm_type == 'picoruby' ? 'mrubyc' : 'mruby'
  gems_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/")
  gems = {
    need_build: [], # C extensions && No platform dependent
    no_need_build: [] # (C extensions && Platform dependent) || No C extensions (possibly has mock)
  }
  Dir.glob("#{gems_dir}/picoruby-*").each do |gem_path|
    next unless Dir.exist?("#{gem_path}/test")
    next if specified_gem && File.basename(gem_path) != specified_gem
    if Dir.exist?("#{gem_path}/src/#{vm}")
      gems[:need_build] << File.basename(gem_path)
    else
      gems[:no_need_build] << File.basename(gem_path)
    end
  end
  gems
end

def create_temp_build_config(base_config_name, gem_name)
  config_file = Tempfile.new(['test_config_', '.rb'])
  base_config_path = File.expand_path("#{MRUBY_ROOT}/build_config/#{base_config_name}")
  config_content = File.read(base_config_path)

  injection_point = /conf\.(picoruby|microruby)/
  injection_text = "conf.gem core: '#{gem_name}'\n  "
  config_content.sub!(injection_point, injection_text + '\1')

  config_file.write(config_content)
  config_file.close
  config_file.path
end

def run_picotest_runner(gem_name, lib_name, load_files)
  gem_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/#{gem_name}")
  test_dir = File.join(gem_dir, 'test')

  unless Dir.exist?(test_dir)
    puts "WARNING: Test directory not found for #{gem_name}. Skipping."
    return true
  end

  puts "Target VM: #{ENV['PICORUBY_TEST_TARGET_VM']}"
  puts "Test directory: #{test_dir}"
  puts "Library to require: #{lib_name || 'none'}"
  puts "Files to load: #{load_files.join(', ')}" unless load_files.empty?

  ENV['RUBY'] = ENV['PICORUBY_TEST_TARGET_VM']

  runner = Picotest::Runner.new(test_dir, nil, "/tmp", lib_name, load_files, gem_dir)
  error_count = runner.run
  return error_count == 0
end

