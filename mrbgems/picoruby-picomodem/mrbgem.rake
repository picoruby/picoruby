MRuby::Gem::Specification.new('picoruby-picomodem') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby Binary Transfer Protocol (RBTP) for device communication'

  spec.add_dependency 'picoruby-crc'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-pack'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-pack"
  end
end
