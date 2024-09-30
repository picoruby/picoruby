MRuby::Gem::Specification.new('picoruby-require') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby require gem'
  spec.add_dependency 'picoruby-vfs'
  spec.add_dependency 'picoruby-filesystem-fat'
  spec.add_dependency 'picoruby-sandbox'

  mrbgems_dir = File.expand_path "..", build_dir

  # picogems to be required in Ruby
  picogems = Hash.new
  task :collect_gems => "#{mrbgems_dir}/gem_init.c" do
    build.gems.each do |gem|
      if gem.name.start_with?("picoruby-")
        gem_name = gem.name.sub(/\Apicoruby-(bin-)?/,'')
        mrbfile = "#{mrbgems_dir}/#{gem.name}/mrblib/#{gem_name}.c"
        src_dir = "#{gem.dir}/src"
        initializer = if Dir.exist?(src_dir) && !Dir.empty?(src_dir)
                        "mrbc_#{gem_name}_init".gsub('-','_')
                      else
                        "NULL"
                      end
        picogems[gem.require_name || File.basename(mrbfile, ".c")] = {
          mrbfile: mrbfile,
          initializer: initializer
        }
        gem.setup
        file mrbfile => gem.rbfiles do |t|
          next if t.prerequisites.empty?
          mkdir_p File.dirname(t.name)
          File.open(t.name, 'w') do |f|
            name = File.basename(t.name, ".c").gsub('-','_')
            mrbc.run(f, t.prerequisites, name, false)
            if initializer != "NULL"
              f.puts
              f.puts "void #{initializer}(mrbc_vm *vm);"
            end
          end
        end
      end
    end
  end

  build.libmruby_objs << objfile("#{mrbgems_dir}/picogem_init")
  file objfile("#{mrbgems_dir}/picogem_init") => ["#{mrbgems_dir}/picogem_init.c"]

  file "#{mrbgems_dir}/picogem_init.c" => [*picogems.values.map{_1[:mrbfile]}, MRUBY_CONFIG, __FILE__, :collect_gems] do |t|
    mkdir_p File.dirname t.name
    open(t.name, 'w+') do |f|
      f.puts <<~PICOGEM
        #include <stdio.h>
        #include <stdbool.h>
        #include <mrubyc.h>
        #include <alloc.h>
      PICOGEM
      f.puts
      picogems.each do |_require_name, v|
        Rake::FileTask[v[:mrbfile]].invoke
        f.puts "#include \"#{v[:mrbfile]}\"" if File.exist?(v[:mrbfile])
      end
      f.puts
      f.puts <<~PICOGEM
        typedef struct picogems {
          const char *name;
          const uint8_t *mrb;
          void (*initializer)(mrbc_vm *vm);
          bool required;
        } picogems;
      PICOGEM
      f.puts
      f.puts "static picogems prebuilt_gems[] = {"
      picogems.each do |require_name, v|
        name = File.basename(v[:mrbfile], ".c")
        f.puts "  {\"#{require_name}\", #{name.gsub('-','_')}, #{v[:initializer]}, false}," if File.exist?(v[:mrbfile])
      end
      f.puts "  {NULL, NULL, NULL, true} /* sentinel */"
      f.puts "};"
      f.puts
      f.puts <<~PICOGEM
        /* public API */
        int
        picoruby_load_model(const uint8_t *mrb)
        {
          mrbc_vm *vm = mrbc_vm_open(NULL);
          if (vm == 0) {
            console_printf("Error: Can't open VM.\\n");
            return 0;
          }
          if (mrbc_load_mrb(vm, mrb) != 0) {
            console_printf("Error: Illegal bytecode.\\n");
            return 0;
          }
          mrbc_vm_begin(vm);
          mrbc_vm_run(vm);
          mrbc_raw_free(vm);
          return 1;
        }

        static int
        gem_index(const char *name)
        {
          if (!name) return -1;
          for (int i = 0; ; i++) {
            if (prebuilt_gems[i].name == NULL) {
              return -1;
            } else if (strcmp(name, prebuilt_gems[i].name) == 0) {
              return i;
            }
          }
        }

        int
        picoruby_load_model_by_name(const char *gem)
        {
          int i = gem_index(gem);
          if (i < 0) return -1;
          return picoruby_load_model(prebuilt_gems[i].mrb);
        }

        static void
        c_extern(mrbc_vm *vm, mrbc_value *v, int argc)
        {
          if (argc == 0 || 2 < argc) {
            mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1..2)");
            return;
          }
          if (GET_TT_ARG(1) != MRBC_TT_STRING) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
            return;
          }
          const char *name = (const char *)GET_STRING_ARG(1);
          int i = gem_index(name);
          if (i < 0) {
            SET_NIL_RETURN();
            return;
          }
          bool force = false;
          if (argc == 2 && GET_TT_ARG(2) == MRBC_TT_TRUE) {
            force = true;
          }
          if ((force || !prebuilt_gems[i].required) && picoruby_load_model(prebuilt_gems[i].mrb)) {
            if (prebuilt_gems[i].initializer) prebuilt_gems[i].initializer(vm);
            prebuilt_gems[i].required = true;
            SET_TRUE_RETURN();
          } else {
            SET_FALSE_RETURN();
          }
        }
      PICOGEM
      f.puts

      f.puts <<~PICOGEM
        void
        picoruby_init_require(void)
        {
          mrbc_value self = mrbc_instance_new(NULL, mrbc_class_object, 0);
          mrbc_value str = mrbc_string_new_cstr(NULL, "require");
          mrbc_value args[2] = { self, str };
          c_extern(NULL, args, 1);
          mrbc_define_method(0, mrbc_class_object, "extern", c_extern);
        }
      PICOGEM
    end
  end

end

