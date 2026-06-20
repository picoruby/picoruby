require 'tempfile'
require_relative "#{MRUBY_ROOT}/mrbgems/picoruby-picotest/mrblib/picotest.rb"

desc "shorthand for test:gems:steep"
task :steep => "test:gems:steep"

desc "shorthand for test:all"
task :test => "test:all"

namespace :test do

  ENV['TEST_TASK'] = "yes"

  desc "run all tests"
  task :all => ["gems:steep", "gems:picoruby", "gems:femtoruby", "gems:wasm"]

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

  task :build_wasm_test do
    puts "Building test runner with picoruby-wasm-test.rb..."
    sh "MRUBY_CONFIG=picoruby-wasm-test rake clean"
    sh "MRUBY_CONFIG=picoruby-wasm-test rake all"
  end

  namespace :gems do
    desc "steep check"
    task :steep do
      sh "bundle exec steep check --log-level=fatal"
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

    desc "run test for a gem on WASM (Node.js runtime)"
    task :wasm, [:specified_gem] do |t, args|
      unless run_test_for_gems('wasm', args[:specified_gem])
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

  if vm_type == 'wasm'
    # WASM uses Node.js runner
    wasm_runner = File.expand_path("#{MRUBY_ROOT}/mrbgems/picoruby-wasm/tools/wasm-runner.mjs")
    ENV['PICORB_TEST_TARGET_VM'] = "node #{wasm_runner}"
  else
    ENV['PICORB_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/#{vm_type}")
  end

  puts "Strategy: Full build for"
  # workaround. TODO: delete this after removal of picoruby-net
  if gems.any?{|gem| gem[:name] == 'picoruby-net' } && gems.any?{|gem| gem[:name] == 'picoruby-socket' }
    gems.reject!{|gem| gem[:name] == 'picoruby-net' }
  end
  gems.each { |gem| puts "  - #{gem[:name]}" }
  config_path = create_temp_build_config("#{vm_type}-test.rb", gems, vm_type)
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
  vm = case vm_type
       when 'femtoruby' then 'mrubyc'
       when 'wasm' then 'mruby'  # WASM uses mruby VM
       else 'mruby'
       end
  build = create_test_collection_build(vm_type)
  gems_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/")
  gems = []
  Dir.glob(["#{gems_dir}/picoruby-*", "#{gems_dir}/mruby-*"]).map do |gem_path|
    next unless Dir.exist?("#{gem_path}/test")
    next if specified_gem && File.basename(gem_path) != specified_gem
    if Dir.exist?("#{gem_path}/src/#{vm}")
      # C extension exists for the target VM
      name = File.basename(gem_path)
    elsif !Dir.exist?("#{gem_path}/src/mruby") && !Dir.exist?("#{gem_path}/src/mrubyc")
      # Only pure Ruby implementation
      name = File.basename(gem_path)
    else
      next
    end
    spec = load_test_gem_spec(build, name)
    next unless spec
    next unless gem_supported_for_test_target?(spec, vm_type)
    test_rbfiles = collect_test_rbfiles(spec, gem_path, vm_type)
    next if test_rbfiles.empty?
    gems << {name: name, require_name: spec.require_name, test_rbfiles: test_rbfiles, spec: spec}
  end
  resolve_gem_conflicts_for_test_target(build, gems, vm_type, specified_gem)
end

def create_test_collection_build(vm_type)
  build_name = "#{vm_type}-test-collection"
  build_dir = "#{MRUBY_ROOT}/build/test-collection"
  build = if vm_type == 'wasm'
    MRuby::CrossBuild.new(build_name, build_dir) do |conf|
      configure_test_collection_build(conf, vm_type)
    end
  else
    MRuby::Build.new(build_name, build_dir, internal: true) do |conf|
      configure_test_collection_build(conf, vm_type)
    end
  end
  build
end

def configure_test_collection_build(conf, vm_type)
  conf.disable_libmruby
  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  if vm_type == 'femtoruby'
    conf.cc.defines << "PICORB_VM_MRUBYC"
  else
    conf.cc.defines << "PICORB_VM_MRUBY"
  end
  if vm_type == 'wasm'
    conf.cc.defines << "PICORB_PLATFORM_WASM"
    conf.cc.command = 'emcc'
  end
end

def load_test_gem_spec(build, name)
  return build.gems[name] if build.gems[name]

  spec = build.gem core: name
  spec&.setup
  spec
rescue => e
  warn "WARNING: Failed to load #{name} for test discovery: #{e.class}: #{e.message}"
  nil
end

def resolve_gem_conflicts_for_test_target(build, gems, vm_type, specified_gem)
  target_specs = test_target_base_gem_names(vm_type).filter_map { |name| load_test_gem_spec(build, name) }
  protected_names = protected_test_gem_names(build, gems, vm_type)
  rejected = {}

  gems.each do |gem|
    next if protected_names.include?(gem[:name])
    next unless conflicts_with_any_name?(gem[:spec], protected_names) ||
                conflicts_with_any_spec?(gem[:spec], target_specs) ||
                target_specs.any? { |target_spec| conflict_declared?(target_spec, gem[:spec]) }

    rejected[gem[:name]] = true
  end

  loop do
    removed = false
    gems.each do |gem|
      next if rejected[gem[:name]]

      gems.each do |other|
        next if gem[:name] == other[:name] || rejected[other[:name]]
        next unless conflict_declared?(gem[:spec], other[:spec])

        rejected[conflict_loser(gem[:name], other[:name], specified_gem)] = true
        removed = true
      end
    end
    break unless removed
  end

  gems.reject { |gem| rejected[gem[:name]] }
end

def protected_test_gem_names(build, gems, vm_type)
  names = test_target_base_gem_names(vm_type)
  gems.each do |gem|
    names.concat(dependency_gem_names(build, gem[:spec]))
  end
  names.uniq
end

def dependency_gem_names(build, spec, seen = {})
  return [] if seen[spec.name]
  seen[spec.name] = true

  spec.dependencies.flat_map do |dependency|
    dependency_spec = load_dependency_spec(build, dependency)
    [dependency[:gem], *(dependency_spec ? dependency_gem_names(build, dependency_spec, seen) : [])]
  end
end

def test_target_base_gem_names(vm_type)
  case vm_type
  when 'wasm'
    ['picoruby-wasm']
  else
    []
  end
end

def conflicts_with_any_name?(spec, names)
  spec.conflicts.any? { |conflict| names.include?(conflict[:gem]) }
end

def conflicts_with_any_spec?(spec, other_specs)
  other_specs.any? { |other_spec| conflict_declared?(spec, other_spec) }
end

def conflict_declared?(spec, other_spec)
  spec.conflicts.any? { |conflict| conflict[:gem] == other_spec.name }
end

def conflict_loser(name, other_name, specified_gem)
  return other_name if specified_gem == name
  return name if specified_gem == other_name

  other_name
end

def gem_supported_for_test_target?(spec, vm_type)
  return false if vm_type != 'wasm' && depends_on_gem?(spec.build, spec, 'picoruby-wasm')
  return false if vm_type == 'femtoruby' && depends_on_gem?(spec.build, spec, 'picoruby-mruby')
  return false if spec.name == 'mruby-compiler2'
  return false if vm_type == 'wasm' && depends_on_gem?(spec.build, spec, 'picoruby-socket')
  return false if vm_type == 'wasm' && depends_on_gem?(spec.build, spec, 'picoruby-net')

  true
end

def depends_on_gem?(build, spec, dependency_name, seen = {})
  return true if spec.name == dependency_name
  return false if seen[spec.name]
  seen[spec.name] = true

  spec.dependencies.any? do |dependency|
    next true if dependency[:gem] == dependency_name

    dependency_spec = load_dependency_spec(build, dependency)
    dependency_spec && depends_on_gem?(build, dependency_spec, dependency_name, seen)
  end
end

def load_dependency_spec(build, dependency)
  return build.gems[dependency[:gem]] if build.gems[dependency[:gem]]

  params = local_dependency_gem_params(dependency)
  return nil unless params

  spec = build.gem(params)
  spec&.setup
  spec
rescue => e
  warn "WARNING: Failed to load dependency #{dependency[:gem]} for test discovery: #{e.class}: #{e.message}"
  nil
end

def local_dependency_gem_params(dependency)
  params = dependency[:default]
  if params
    if params[:core]
      return params if File.directory?("#{MRUBY_ROOT}/mrbgems/#{params[:core]}")
    elsif params[:gemdir]
      return params if File.directory?(File.expand_path(params[:gemdir]))
    end
    return nil
  end

  gem_dir = "#{MRUBY_ROOT}/mrbgems/#{dependency[:gem]}"
  return { core: dependency[:gem] } if File.directory?(gem_dir)

  nil
end

def collect_test_rbfiles(spec, gem_path, vm_type)
  if spec.instance_variable_defined?(:@test_rbfiles)
    return Array(spec.test_rbfiles)
  end

  Array(spec.test_rbfiles)
end

def create_temp_build_config(base_config_name, gems, vm_type = nil)
  config_file = Tempfile.new(['test_config_', '.rb'])

  # Handle wasm-test specially since it uses a different base config
  if vm_type == 'wasm'
    base_config_name = 'picoruby-wasm-test.rb'
  end

  base_config_path = File.expand_path("#{MRUBY_ROOT}/build_config/#{base_config_name}")
  config_content = File.read(base_config_path)

  if vm_type == 'wasm'
    # For WASM, inject gems before the final 'end'
    injection_text = gems.map { |gem| "  conf.gem core: '#{gem[:name]}'" }.join("\n") + "\n"
    config_content.sub!(/^end$/, injection_text + "end")
  else
    injection_point = /conf\.(femtoruby|picoruby)/
    injection_text = gems.map { |gem| "conf.gem core: '#{gem[:name]}'" }.join("\n  ") + "\n  "
    config_content.sub!(injection_point, injection_text + 'conf.\1')
  end

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
  puts "Test files: #{gem[:test_rbfiles].join(', ')}" if gem[:test_rbfiles]

  ENV['RUBY'] = ENV['PICORB_TEST_TARGET_VM']

  runner = Picotest::Runner.new(test_dir, tmpdir: "/tmp", require_name: lib_name, load_files: load_files, load_path: gem_dir, entries: gem[:test_rbfiles])
  error_count = runner.run
  return error_count == 0
end
