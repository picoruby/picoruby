require 'erb'

MRuby::Gem::Specification.new('picoruby-require') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby require gem'

  spec.add_dependency 'picoruby-sandbox'

  if build.vm_mrubyc?
    if build.posix?
      # TODO: in Wasm, you may need to implement File class with File System Access API
      spec.add_dependency 'picoruby-posix-io'
    else
      spec.add_dependency 'picoruby-vfs'
      spec.add_dependency 'picoruby-filesystem-fat'
    end
  end

  mrbgems_dir = File.expand_path "..", build_dir

  # picogems to be required in Ruby
  picogems = Hash.new
  mgems = Array.new
  task :collect_gems => "#{mrbgems_dir}/gem_init.c" do
    build.gems.each do |gem|
      if gem.name.start_with?("picoruby-") && !gem.name.start_with?("picoruby-bin-")
        gem_name = gem.name.sub(/\Apicoruby-?/,'')
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
        rbfiles = Dir.glob("#{gem.dir}/mrblib/**/*.rb").sort
        file mrbfile => rbfiles do |t|
          next if t.prerequisites.empty?
          mkdir_p File.dirname(t.name)
          File.open(t.name, 'w') do |f|
            name = File.basename(t.name, ".c").gsub('-','_')
            mrbc.run(f, t.prerequisites, name, cdump: false)
            if initializer != "NULL"
              f.puts
              f.puts "void #{initializer}(mrbc_vm *vm);"
            end
          end
        end
      elsif gem.name.start_with?("mruby-")
        mgems << gem.name.sub(/\Amruby-?/,'')
      end
    end
  end

  build.libmruby_objs << objfile("#{mrbgems_dir}/picogem_init")
  file objfile("#{mrbgems_dir}/picogem_init") => ["#{mrbgems_dir}/picogem_init.c"]

  file "#{mrbgems_dir}/picogem_init.c" => [*picogems.values.map{_1[:mrbfile]}, MRUBY_CONFIG, __FILE__, :collect_gems] do |t|
    picogems.each do |_require_name, v|
      Rake::FileTask[v[:mrbfile]].invoke
    end
    template_path = if build.vm_mruby?
                      File.join(spec.dir, "templates/mruby/picogem_init.c.erb")
                    else
                      File.join(spec.dir, "templates/mrubyc/picogem_init.c.erb")
                    end
    template = ERB.new(File.read(template_path), trim_mode: "%-")
    File.write(t.name, template.result(binding))
  end

end

