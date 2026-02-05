MRuby::Gem::Specification.new('picoruby-usb-hid') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'USB HID (Keyboard, Mouse, Consumer Control) library'

  spec.require_name = 'usb/hid'

  spec.add_dependency 'picoruby-machine'

  # Setup paths
  keycode_txt = "#{dir}/keycode.txt"
  keycode_inc = "#{build_dir}/keycode.inc"

  # Add build directory to include paths
  cc.include_paths << build_dir

  # Define directory task
  directory build_dir

  # Define file generation task
  file keycode_inc => [keycode_txt, build_dir] do
    # Read keycode.txt
    keycodes = File.readlines(keycode_txt).map(&:chomp).reject(&:empty?)

    # Generate keycode.inc
    File.open(keycode_inc, 'w') do |f|
      f.puts "// Auto-generated from keycode.txt - DO NOT EDIT"
      f.puts ""
      f.puts "#if defined(PICORB_VM_MRUBY)"
      f.puts ""
      keycodes.each do |line|
        name, value = line.split(',')
        f.puts "  mrb_define_const_id(mrb, keycode_module, MRB_SYM(#{name}), mrb_fixnum_value(#{value}));"
      end
      f.puts ""
      f.puts "#elif defined(PICORB_VM_MRUBYC)"
      f.puts ""
      keycodes.each do |line|
        name, value = line.split(',')
        f.puts "  SET_CLASS_CONST(mrbc_module_Keycode, #{name}, #{value});"
      end
      f.puts ""
      f.puts "#endif"
    end
  end

  # Invoke task when building
  tasks = Rake.application.top_level_tasks
  if (tasks & %w(default build all)).any?
    Rake::Task[keycode_inc].invoke
  end
end
