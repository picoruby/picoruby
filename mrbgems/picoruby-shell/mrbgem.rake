require 'stringio'
require 'zlib'

MRuby::Gem::Specification.new('picoruby-shell') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby Shell library'

  spec.add_dependency 'picoruby-require'
  spec.add_dependency 'picoruby-editor'
  spec.add_dependency 'picoruby-sandbox'
  spec.add_dependency 'picoruby-env'
  spec.add_dependency 'picoruby-crc'
  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-yaml'
  spec.add_dependency 'picoruby-data'
  if build.posix?
    if build.vm_mrubyc?
      spec.add_dependency 'picoruby-dir'
    elsif build.vm_mruby?
      spec.add_dependency 'mruby-dir', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-dir"
    end
  end

  exe_dir = "#{build_dir}/shell_executables"
  if Dir.exist?(exe_dir)
    Dir.each_child(exe_dir) do |filename|
      filepath = File.join(exe_dir, filename)
      FileUtils.rm(filepath)
    end
  end

  executables_src = "#{build_dir}/shell_executables.c.inc"
  if File.exist?(executables_src)
    File.delete(executables_src)
  end
  cc.include_paths << build_dir

  executable_mrbfiles = Array.new
  executable_dir = "#{build_dir}/shell_executables"
  directory executable_dir
  Dir.glob("#{dir}/shell_executables/*.rb") do |rbfile|
    mrbfile = "#{executable_dir}/#{rbfile.pathmap('%n')}.c"
    file mrbfile => [rbfile, executable_dir, executables_src] do |t|
      File.open(t.name, 'w') do |f|
        mrbc.compile_options = "--remove-lv -B%{funcname} -o-"
        mrbc.run(f, t.prerequisites[0], "executable_#{t.name.pathmap("%n").gsub('-', '_')}", cdump: false)
      end
    end
    executable_mrbfiles << { mrbfile: mrbfile, rbfile: rbfile }
    objfile = "#{build_dir}/shell_executables/#{rbfile.pathmap('%n')}.o"
    file objfile => mrbfile
    build.libmruby_objs << objfile
  end

  file executables_src do |t|
    mkdir_p File.dirname t.name
    pathmap = File.read("#{dir}/shell_executables/_path.txt").lines.map(&:chomp).map do
      pn = Pathname.new(_1)
      { dir: pn.dirname.to_s, basename: pn.basename.to_s }
    end
    open(t.name, 'w') do |f|
      executable_mrbfiles.each do |vm_code|
        Rake::FileTask[vm_code[:mrbfile]].invoke
        f.puts "#include \"#{vm_code[:mrbfile]}\""
      end
      f.puts
      f.puts "static shell_executables executables[] = {"
      executable_mrbfiles.each do |vm_code|
        sio = StringIO.new("hoge", 'w+')
        sio.set_encoding('ASCII-8BIT')
        mrbc.compile_options = "--remove-lv -o-"
        mrbc.run(sio, vm_code[:rbfile], "", cdump: false)
        sio.rewind
        crc = Zlib.crc32(sio.read.chomp)
        sio.close
        basename = File.basename(vm_code[:mrbfile], ".c")
        dirname = pathmap.find { _1[:basename] == basename }[:dir]
        line = "  {\"#{dirname}/#{basename}\", executable_#{basename.gsub('-', '_')}, #{crc}},"
        f.puts line
      end
      f.puts "  {NULL, NULL} /* sentinel */"
      f.puts "};"
    end
  end

end
