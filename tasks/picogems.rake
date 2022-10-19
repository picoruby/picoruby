MRuby.each_target do |build|

  mrbfiles = Array.new
  gems.each do |gem|
    if gem.name.start_with?("picoruby-")
      mrbfile = "#{build_dir}/mrbgems/#{gem.name}/mrblib/#{gem.name.sub(/\Apicoruby-(bin-)?/,'')}.c"
      mrbfiles << mrbfile
      file mrbfile => gem.rbfiles do |t|
        next if t.prerequisites.empty?
        mkdir_p File.dirname(t.name)
        File.open(t.name, 'w') do |f|
          name = File.basename(t.name, ".c")
          mrbc.run(f, t.prerequisites, name, false)
          f.puts
          f.puts "void mrbc_#{name}_init();"
        end
      end
    end
  end

  self.libmruby_objs << objfile("#{build_dir}/mrbgems/picogem_init")
  file objfile("#{build_dir}/mrbgems/picogem_init") => ["#{build_dir}/mrbgems/picogem_init.c"]

  file "#{build_dir}/mrbgems/picogem_init.c" => [*mrbfiles, MRUBY_CONFIG, __FILE__] do |t|
    mkdir_p File.dirname t.name
    open(t.name, 'w+') do |f|
      f.puts <<~PICOGEM
        #include <stdio.h>
        #include <mrubyc.h>
      PICOGEM
      f.puts
      mrbfiles.each do |mrb|
        f.puts "#include \"#{mrb}\"" if File.exist?(mrb)
      end
      f.puts
      f.puts <<~PICOGEM
        typedef struct picogems {
          const char *name;
          const uint8_t *mrb;
          void (*initializer)(void);
          int required;
        } picogems;
      PICOGEM
      f.puts
      f.puts "static picogems gems[] = {"
      mrbfiles.each do |mrb|
        name = File.basename(mrb, ".c")
        f.puts "  {\"#{name}\", #{name}, mrbc_#{name}_init, 0}," if File.exist?(mrb)
      end
      f.puts "  {NULL, NULL, NULL, 1} /* sentinel */"
      f.puts "};"
      f.puts
      f.puts <<~PICOGEM
        static int
        gem_index(const char *name)
        {
          if (!name) return -1;
          for (int i = 0; ; i++) {
            if (gems[i].name == NULL) {
              return -1;
            } else if (strcmp(name, gems[i].name) == 0) {
              return i;
            }
          }
        }

        static int
        load_model(const uint8_t *mrb)
        {
          mrbc_vm *vm = mrbc_vm_open(NULL);
          if (vm == 0) {
            console_printf("Error: Can't open VM.");
            return 0;
          }
          if (mrbc_load_mrb(vm, mrb) != 0) {
            console_printf("Error: Illegal bytecode.");
            return 0;
          }
          mrbc_vm_begin(vm);
          mrbc_vm_run(vm);
          mrbc_raw_free(vm);
          return 1;
        }

        static void
        c_require(mrb_vm *vm, mrb_value *v, int argc)
        {
          const char *name = (const char *)GET_STRING_ARG(1);
          int i = gem_index(name);
          if (i < 0) {
            char buff[64];
            sprintf(buff, "cannot find such gem -- %s", name);
            mrbc_raise(vm, MRBC_CLASS(RuntimeError), buff);
            return;
          }
          if (!gems[i].required && load_model(gems[i].mrb)) {
            if (gems[i].initializer) gems[i].initializer();
            gems[i].required = 1;
            SET_TRUE_RETURN();
          } else {
            SET_FALSE_RETURN();
          }
        }

        void
        mrbc_require_init(void)
        {
          mrbc_define_method(0, mrbc_class_object, "require", c_require);
        }
      PICOGEM
    end
  end
end
