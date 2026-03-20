MRuby::Gem::Specification.new('picoruby-net-ntp') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'NTP (Network Time Protocol) client for PicoRuby'
  spec.description = 'Simple NTP client implementation compatible with CRuby'

  spec.require_name = 'net/ntp'

  if build.vm_mruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/picoruby-pack"
  elsif build.vm_mrubyc?
    spec.add_dependency 'picoruby-pack'
  end
  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-time'
end
