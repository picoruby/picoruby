MRuby::Gem::Specification.new('picoruby-mruby') do |spec|
  spec.license = ['MIT', 'Apache-2.0', 'BSD-3-Clause', 'BSD']
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby library'

  spec.add_conflict 'picoruby-mrubyc'

  # Use mruby-task gem instead of src/task.c
  # NOTE: src/task.c has been moved to deprecated/task.c.bak
  # Load HAL gem first so mruby-task can find it
  if build.posix?
    spec.add_dependency 'mruby-io', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  end
  spec.add_dependency 'mruby-task', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-task"

  # Default to a 64-bit mrb_int without boxing unless the build config has
  # chosen both itself. The 32-bit targets pick MRB_INT32 with
  # MRB_WORD_BOXING, and mrbconf.h rejects MRB_INT64 beside a boxing on
  # a 32-bit build.
  unless build.cc.defines.any? { |d| d =~ /\AMRB_INT(32|64)\z/ }
    build.cc.defines << "MRB_INT64"
  end
  unless build.cc.defines.any? { |d| d =~ /\AMRB_(NO|WORD|NAN)_BOXING\z/ }
    build.cc.defines << "MRB_NO_BOXING"
  end
  build.cc.defines << "MRB_UTF8_STRING"

  # MRB_BASELINE_PROFILE and MRB_CONSTRAINED_BASELINE_PROFILE define MRB_NO_METHOD_CACHE
  # and it varies sizeof(mrb_state), so they should be build-wide define.
  if spec.build.wasm? || spec.build.posix?
    spec.build.defines << "MRB_BASELINE_PROFILE=1"
  else
    spec.cc.defines << "MRB_HEAP_PAGE_SIZE=128"
    spec.build.defines << "MRB_CONSTRAINED_BASELINE_PROFILE=1"
  end

  dir_for_enhanced_rule = "lib/mruby/src"
  Dir.glob(File.join(dir, "#{dir_for_enhanced_rule}/*.c")).each do |file|
    if build.cc.defines.include?("PICORB_ALLOC_ESTALLOC") && File.basename(file) == "allocf.c"
      next
    end
    obj = objfile(file.pathmap("#{build_dir}/#{dir_for_enhanced_rule}/%n"))
    build.libmruby_objs << obj
    file obj => [file] do |t|
      cc.run t.name, t.prerequisites.first
    end
  end
end
