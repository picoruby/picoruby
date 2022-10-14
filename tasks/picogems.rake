MRuby.each_target do |build|

  mrbfiles = Array.new
  gems.each do |gem|
    if gem.name.start_with?("picoruby-") && !gem.name.start_with?("picoruby-bin-")
      mrbfile = "#{build_dir}/mrbgems/#{gem.name}/mrblib/#{gem.name.sub(/\Apicoruby-/,'')}.c"
      mrbfiles << mrbfile
      file mrbfile => gem.rbfiles do |t|
        mkdir_p File.dirname(t.name)
        File.open(t.name, 'w') do |f|
          name = File.basename(t.name, ".c")
          mrbc.run(f, t.prerequisites, name, false)
          f.puts
          f.puts "void c_#{name}_init(mrbc_vm *vm);"
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
        f.puts "#include \"#{mrb}\""
      end
      f.puts
      f.puts <<~PICOGEM
        typedef struct picogems {
          const char *name;
          const uint8_t *mrb;
          void (*initializer)(mrbc_vm *);
          int required;
        } picogems;
      PICOGEM
      f.puts
      f.puts "static picogems gems[] = {"
      mrbfiles.each do |mrb|
        name = File.basename(mrb, ".c")
        f.puts "  {\"#{name}\", #{name}, c_#{name}_init, 0},"
      end
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

        void
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
          if (!gems[i].required && mrbc_load_mrb(vm, gems[i].mrb) == 0) {
            if (gems[i].initializer) gems[i].initializer(vm);
            gems[i].required = 1;
            SET_TRUE_RETURN();
          } else {
            SET_FALSE_RETURN();
          }
        }
      PICOGEM
    end
  end
end
