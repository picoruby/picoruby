require 'tempfile'
require_relative "#{MRUBY_ROOT}/mrbgems/picoruby-picotest/mrblib/picotest.rb"

desc "shorthand for test:gems:steep"
task :steep => "test:gems:steep"

desc "shorthand for test:all"
task :test => "test:all"

namespace :test do

  ENV['TEST_TASK'] = "yes"

  desc "run all tests"
  task :all => ["gems:steep", "gems:picoruby", "gems:femtoruby"]

  task :build_femtoruby_test do
    puts "Building test runner with femtoruby-test.rb..."
    sh "PICORB_DEBUG=yes MRUBY_CONFIG=femtoruby-test rake clean"
    sh "PICORB_DEBUG=yes MRUBY_CONFIG=femtoruby-test rake all"
  end

  task :build_picoruby_test do
    puts "Building test runner with picoruby-test.rb..."
    sh "PICORB_DEBUG=yes MRUBY_CONFIG=picoruby-test rake clean"
    sh "PICORB_DEBUG=yes MRUBY_CONFIG=picoruby-test rake all"
  end

  namespace :gems do
    desc "steep check"
    task :steep do
      sh "bundle exec steep check"
    end

    desc "run test for a gem on FemtoRuby"
    task :femtoruby, [:specified_gem] do |t, args|
      unless run_test_for_gems('femtoruby', args[:specified_gem])
        exit 1
      end
    end

    desc "run test for a gem on PicoRuby"
    task :picoruby, [:specified_gem] do |t, args|
      unless run_test_for_gems('picoruby', args[:specified_gem])
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
  ENV['PICORB_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/#{vm_type}")
  puts "Strategy: Full build for"
  # workaround. TODO: delete this after removal of picoruby-net
  if gems.any?{|gem| gem[:name] == 'picoruby-net' } && gems.any?{|gem| gem[:name] == 'picoruby-socket' }
    gems.reject!{|gem| gem[:name] == 'picoruby-net' }
  end
  gems.each { |gem| puts "  - #{gem[:name]}" }
  config_path = create_temp_build_config("#{vm_type}-test.rb", gems)
  puts "Building test binary on #{vm_type}..."
  unless ENV['SKIP_BUILD']
    sh "PICORB_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
    sh "PICORB_DEBUG=1 MRUBY_CONFIG=#{config_path} rake all"
  end
  gems.each do |gem|
    unless run_picotest_runner(gem, [])
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
  vm = vm_type == 'femtoruby' ? 'mrubyc' : 'mruby'
  gems_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/")
  gems = []
  Dir.glob(["#{gems_dir}/picoruby-*", "#{gems_dir}/mruby-*"]).map do |gem_path|
    next unless Dir.exist?("#{gem_path}/test")
    next if specified_gem && File.basename(gem_path) != specified_gem
    if File.exist?("#{gem_path}/test/target_vm")
      next unless File.read("#{gem_path}/test/target_vm").chomp == vm_type
    end
    if Dir.exist?("#{gem_path}/src/#{vm}")
      # C extension exists for the target VM
      name = File.basename(gem_path)
    elsif !Dir.exist?("#{gem_path}/src/mruby") && !Dir.exist?("#{gem_path}/src/mrubyc")
      # Only pure Ruby implementation
      name = File.basename(gem_path)
    else
      next
    end
    matchdata = File.read("#{gem_path}/mrbgem.rake").match(/require_name\s*=\s*['"](.+)['"]/)
    gems << {name: name, require_name: matchdata ? matchdata[1] : nil}
  end
  gems
end

def create_temp_build_config(base_config_name, gems)
  config_file = Tempfile.new(['test_config_', '.rb'])
  base_config_path = File.expand_path("#{MRUBY_ROOT}/build_config/#{base_config_name}")
  config_content = File.read(base_config_path)

  injection_point = /conf\.(femtoruby|picoruby)/
  injection_text = gems.map { |gem| "conf.gem core: '#{gem[:name]}'" }.join("\n  ") + "\n  "
  config_content.sub!(injection_point, injection_text + 'conf.\1')

  config_file.write(config_content)
  config_file.close
  config_file.path
end

def run_picotest_runner(gem, load_files)
  gem_name = gem[:name]
  gem_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/#{gem_name}")
  test_dir = File.join(gem_dir, 'test')

  unless Dir.exist?(test_dir)
    puts "WARNING: Test directory not found for #{gem_name}. Skipping."
    return true
  end

  lib_name = gem[:require_name] || gem_name.sub(/^picoruby-/, '')

  puts "Target VM: #{ENV['PICORB_TEST_TARGET_VM']}"
  puts "Test directory: #{test_dir}"
  puts "Library to require: #{lib_name}"
  puts "Files to load: #{load_files.join(', ')}" unless load_files.empty?

  ENV['RUBY'] = ENV['PICORB_TEST_TARGET_VM']

  runner = Picotest::Runner.new(test_dir, tmpdir: "/tmp", require_name: lib_name, load_files: load_files, load_path: gem_dir)
  error_count = runner.run
  return error_count == 0
end
