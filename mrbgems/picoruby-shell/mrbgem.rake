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
    if build.vm_mrubyc?
      spec.add_dependency('picoruby-dir')
    elsif build.vm_mruby?
      spec.add_dependency('mruby-dir')
    end
  else
    spec.add_dependency 'picoruby-machine' # for shell executables
  end

  executables_src = "#{build_dir}/shell_executables.c.inc"
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
    executable_mrbfiles << mrbfile
    objfile = "#{build_dir}/shell_executables/#{rbfile.pathmap('%n')}.o"
    file objfile => mrbfile
    build.libmruby_objs << objfile
  end

  file executables_src do |t|
    mkdir_p File.dirname t.name
    pathmap = File.read("#{dir}/shell_executables/_path.txt").lines.map(&:chomp).map do
      p = Pathname.new(_1)
      { dir: p.dirname.to_s, basename: p.basename.to_s }
    end
    open(t.name, 'w+') do |f|
      executable_mrbfiles.each do |vm_code|
        Rake::FileTask[vm_code].invoke
        f.puts "#include \"#{vm_code}\"" if File.exist?(vm_code)
      end
      f.puts
      f.puts "static shell_executables executables[] = {"
      executable_mrbfiles.each do |vm_code|
        basename = File.basename(vm_code, ".c")
        dirname = pathmap.find { _1[:basename] == basename }[:dir]
        f.puts "  {\"#{dirname}/#{basename}\", executable_#{basename.gsub('-', '_')}}," if File.exist?(vm_code)
      end
      f.puts "  {NULL, NULL} /* sentinel */"
      f.puts "};"
    end
  end

end
