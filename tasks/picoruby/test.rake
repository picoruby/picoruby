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
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=picoruby-test rake all"
  end

  task :build_microruby_test do
    puts "Building test runner with microruby-test.rb..."
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=microruby-test rake clean"
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=microruby-test rake all"
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
  if gems.empty?
    puts "No gems found for testing."
    return false
  end
  ENV['PICORUBY_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/#{vm_type}")
  puts "Strategy: Full build for"
  gems.each { |gem_name| puts "  - #{gem_name}" }
  config_path = create_temp_build_config("#{vm_type}-test.rb", gems)
  puts "Building test binary on #{vm_type}..."
  sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
  sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake all"
  gems.each do |gem_name|
    unless run_picotest_runner(gem_name, [])
      all_success = false
    end
  rescue
    all_success = false
  end
  unless all_success
    print Picotest::RED
    puts "Some tests failed. Please check the output above."
    print Picotest::RESET
  end
  return all_success
ensure
  File.unlink(config_path) if config_path && File.exist?(config_path)
end

def collect_gems(vm_type, specified_gem = nil)
  vm = vm_type == 'picoruby' ? 'mrubyc' : 'mruby'
  gems_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/")
  gems = []
  Dir.glob("#{gems_dir}/picoruby-*").map do |gem_path|
    next unless Dir.exist?("#{gem_path}/test")
    next unless Dir.exist?("#{gem_path}/src/#{vm}")
    next if specified_gem && File.basename(gem_path) != specified_gem
    if Dir.exist?("#{gem_path}/src/#{vm}")
      # C extension exists for the target VM
      File.basename(gem_path)
    elsif !Dir.exist?("#{gem_path}/src/mruby") && !Dir.exist?("#{gem_path}/src/mrubyc")
      # Only pure Ruby implementation
      File.basename(gem_path)
    else
      nil
    end
  end.compact
end

def create_temp_build_config(base_config_name, gems)
  config_file = Tempfile.new(['test_config_', '.rb'])
  base_config_path = File.expand_path("#{MRUBY_ROOT}/build_config/#{base_config_name}")
  config_content = File.read(base_config_path)

  injection_point = /conf\.(picoruby|microruby)/
  injection_text = gems.map { |gem_name| "conf.gem core: '#{gem_name}'" }.join("\n  ") + "\n  "
  config_content.sub!(injection_point, injection_text + '\1')

  config_file.write(config_content)
  config_file.close
  config_file.path
end

def run_picotest_runner(gem_name, load_files)
  gem_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/#{gem_name}")
  test_dir = File.join(gem_dir, 'test')

  unless Dir.exist?(test_dir)
    puts "WARNING: Test directory not found for #{gem_name}. Skipping."
    return true
  end

  lib_name = gem_name.sub(/^picoruby-/, '')

  puts "Target VM: #{ENV['PICORUBY_TEST_TARGET_VM']}"
  puts "Test directory: #{test_dir}"
  puts "Library to require: #{lib_name}"
  puts "Files to load: #{load_files.join(', ')}" unless load_files.empty?

  ENV['RUBY'] = ENV['PICORUBY_TEST_TARGET_VM']

  runner = Picotest::Runner.new(test_dir, nil, "/tmp", lib_name, load_files, gem_dir)
  error_count = runner.run
  return error_count == 0
end

