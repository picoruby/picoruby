MRuby::Gem::Specification.new('picoruby-regexp_light') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Lightweight regular expression engine for microcontrollers. ASCII only.'

  spec.require_name = 'regexp'

  spec.cc.defines << "REGEX_USE_ALLOC_LIBC"
  spec.cc.include_paths << "#{dir}/lib/regex_light/src"

  Dir.glob("#{dir}/lib/regex_light/src/*.c").each do |src|
    obj = "#{build_dir}/src/#{objfile(File.basename(src, ".c"))}"
    file obj => src do |t|
      spec.cc.run t.name, t.prerequisites[0]
    end
    spec.objs << obj
  end
end
