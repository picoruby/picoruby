require 'erb'

MRuby::Gem::Specification.new('picoruby-require') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby require gem'

  if build.vm_mrubyc?
    if cc.defines.flatten.any?{ _1.match? /\AMRBC_USE_HAL_POSIX(=\d+)?\z/ }
      spec.add_dependency 'picoruby-io'
    else
      spec.add_dependency 'picoruby-vfs'
      spec.add_dependency 'picoruby-filesystem-fat'
    end
    spec.add_dependency 'picoruby-sandbox'
  end

  mrbgems_dir = File.expand_path "..", build_dir

  picogems = Hash.new
  task :collect_gems do
    build.gems.each do |gem|
      rbfiles = Dir.glob("#{gem.dir}/mrblib/**/*.rb").sort
      next if rbfiles.empty? # Skip gems with no Ruby files in mrblib

      if gem.name.start_with?("picoruby-") && !gem.name.start_with?("picoruby-bin-")
        gem_name = gem.name.sub(/\Apicoruby-?/,'')
        mrbfile = "#{mrbgems_dir}/#{gem.name}/mrblib/#{gem_name}.c"
        src_dir = "#{gem.dir}/src"
        
        initializer = "NULL"
        if Dir.exist?(src_dir) && !Dir.empty?(src_dir)
          if build.vm_mrubyc?
            initializer = "mrbc_#{gem_name}_init".gsub('-','_')
          elsif build.vm_mruby?
            initializer = "mrb_#{gem.name.gsub('-', '_')}_gem_init"
          end
        end

        picogems[gem.require_name || File.basename(mrbfile, ".c")] = {
          mrbfile: mrbfile,
          initializer: initializer
        }
        
        file mrbfile => rbfiles do |t|
          next if t.prerequisites.empty?
          mkdir_p File.dirname(t.name)
          File.open(t.name, 'w') do |f|
            name = File.basename(t.name, ".c").gsub('-','_')
            mrbc.run(f, t.prerequisites, name, cdump: false)
            if initializer != "NULL" && build.vm_mrubyc?
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

  file "#{mrbgems_dir}/picogem_init.c" => [MRUBY_CONFIG, __FILE__] do |t|
    Rake::Task[:collect_gems].invoke
    
    template_path = if build.vm_mrubyc?
                      "#{spec.dir}/templates/mrubyc/picogem_init.c.erb"
                    elsif build.vm_mruby?
                      "#{spec.dir}/templates/mruby/picogem_init.c.erb"
                    end

    if template_path && File.exist?(template_path)
      # Ensure all dependent .c files are created before rendering the template
      picogems.each { |_, v| Rake::FileTask[v[:mrbfile]].invoke }
      
      template = ERB.new(File.read(template_path), trim_mode: "-")
      code = template.result(binding)
      
      mkdir_p File.dirname(t.name)
      File.open(t.name, 'w+') { |f| f.write(code) }
    end
  end
end