MRuby::Gem::Specification.new('picoruby-shell') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby Shell library'

  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-require'
  end
  spec.add_dependency 'picoruby-editor'
  spec.add_dependency 'picoruby-sandbox'
  spec.add_dependency 'picoruby-env'
  if build.posix?
    if build.cc.defines.include?("PICORB_VM_MRUBYC")
      spec.add_dependency('picoruby-dir')
    elsif build.cc.defines.include?("PICORB_VM_MRUBY")
      spec.add_dependency('mruby-dir')
    end
  else
    spec.add_dependency 'picoruby-machine' # for shell executables
  end

  executable_mrbfiles = Array.new
  executable_dir = "#{build_dir}/shell_executables"
  directory executable_dir
  Dir.glob("#{dir}/shell_executables/*.rb") do |rbfile|
    mrbfile = "#{executable_dir}/#{rbfile.pathmap('%n')}.c"
    file mrbfile => [rbfile, executable_dir] do |t|
      File.open(t.name, 'w') do |f|
        mrbc.compile_options = "--remove-lv -B%{funcname} -o-"
        mrbc.run(f, t.prerequisites[0], "executable_#{t.name.pathmap("%n").gsub('-', '_')}", cdump: false)
      end
    end
    executable_mrbfiles << mrbfile
  end

  mrbgems_dir = File.expand_path "..", build_dir

  init_src = "#{build_dir}/executables_init.c"
  init_obj = objfile(init_src.pathmap('%X'))
  build.libmruby_objs << init_obj
  file init_obj => init_src do |t|
    build.cc.run(t.name, t.prerequisites[0])
  end
  file init_src => executable_mrbfiles do |t|
    mkdir_p File.dirname t.name
    pathmap = File.read("#{dir}/shell_executables/_path.txt").lines.map(&:chomp).map do
      p = Pathname.new(_1)
      { dir: p.dirname.to_s, basename: p.basename.to_s }
    end
    open(t.name, 'w+') do |f|
      f.puts <<~PICOGEM
        #include <stdio.h>
        #include <stdbool.h>
      PICOGEM
      if build.cc.defines.include?("PICORB_VM_MRUBYC")
        f.puts "#include <mrubyc.h>"
      elsif build.cc.defines.include?("PICORB_VM_MRUBY")
        f.puts "#include <mruby.h>"
        f.puts "#include <mruby/hash.h>"
        f.puts "#include <mruby/string.h>"
        f.puts "#include <mruby/presym.h>"
      end
      # shell executables
      executable_mrbfiles.each do |vm_code|
        Rake::FileTask[vm_code].invoke
        f.puts "#include \"#{vm_code}\"" if File.exist?(vm_code)
      end
      f.puts
      f.puts <<~PICOGEM
        typedef struct shell_executables {
          const char *path;
          const uint8_t *vm_code;
        } shell_executables;
      PICOGEM
      f.puts
      f.puts "static shell_executables executables[] = {"
      executable_mrbfiles.each do |vm_code|
        basename = File.basename(vm_code, ".c")
        dirname = pathmap.find { _1[:basename] == basename }[:dir]
        f.puts "  {\"#{dirname}/#{basename}\", executable_#{basename.gsub('-', '_')}}," if File.exist?(vm_code)
      end
      f.puts "  {NULL, NULL} /* sentinel */"
      f.puts "};"
      f.puts
      if build.cc.defines.include?("PICORB_VM_MRUBYC")
        f.puts <<~PICOGEM
          static void
          c_next_executable(mrbc_vm *vm, mrbc_value *v, int argc)
          {
            static int i = 0;
            if (executables[i].path) {
              const uint8_t *vm_code = executables[i].vm_code;
              mrbc_value hash = mrbc_hash_new(vm, 2);
              mrbc_value path = mrbc_string_new_cstr(vm, (char *)executables[i].path);
              mrbc_hash_set(&hash,
                &mrbc_symbol_value(mrbc_str_to_symid("path")),
                &path
              );
              uint32_t codesize = (vm_code[8] << 24) + (vm_code[9] << 16) + (vm_code[10] << 8) + vm_code[11];
              mrbc_value code_val = mrbc_string_new(vm, vm_code, codesize);
              mrbc_hash_set(&hash,
                &mrbc_symbol_value(mrbc_str_to_symid("code")),
                &code_val
              );
              SET_RETURN(hash);
              i++;
            } else {
              SET_NIL_RETURN();
            }
          }
        PICOGEM
        f.puts <<~PICOGEM
          void
          picoruby_init_executables(mrbc_vm *vm)
          {
            mrbc_class *mrbc_class_Shell = mrbc_define_class(vm, "Shell", mrbc_class_object);
            mrbc_define_method(vm, mrbc_class_Shell, "next_executable", c_next_executable);
          }
        PICOGEM
      elsif build.cc.defines.include?("PICORB_VM_MRUBY")
        f.puts <<~PICOGEM
          mrb_value
          mrb_next_executable(mrb_state *mrb, mrb_value self)
          {
            static int i = 0;
            if (executables[i].path) {
              const uint8_t *vm_code = executables[i].vm_code;
              mrb_value hash = mrb_hash_new_capa(mrb, 2);
              mrb_value path = mrb_str_new_cstr(mrb, (char *)executables[i].path);
              mrb_hash_set(mrb, hash,
                          mrb_symbol_value(mrb_intern_cstr(mrb, "path")),
                          mrb_str_dup(mrb, path));
              uint32_t codesize = (vm_code[8] << 24) + (vm_code[9] << 16) + (vm_code[10] << 8) + vm_code[11];
              mrb_value code_val = mrb_str_new(mrb, (char *)vm_code, codesize);
              mrb_hash_set(mrb, hash,
                          mrb_symbol_value(mrb_intern_cstr(mrb, "code")),
                          mrb_str_dup(mrb, code_val));
              i++;
              return hash;
            } else {
              return mrb_nil_value();
            }
          }
        PICOGEM
        f.puts <<~PICOGEM
          void
          picoruby_init_executables(mrb_state *mrb)
          {
            struct RClass *class_Shell = mrb_define_class_id(mrb, MRB_SYM(Shell), mrb->object_class);
            mrb_define_method_id(mrb, class_Shell, MRB_SYM(next_executable), mrb_next_executable, MRB_ARGS_NONE());
          }
        PICOGEM
      end
    end
  end
end
