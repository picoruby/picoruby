MRuby::Gem::Specification.new('picoruby-marshal') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Marshal for PicoRuby (for dRuby support)'
  # dump(obj) takes any marshalable value: Spinel widens a parameter it
  # sees more than one kind of value in, so dump, dump_object and the
  # load_* family are widened defs. Without this flag 6 of 19 defs
  # compile; with it 18 of 19 (encode_fixnum keeps a binary literal).
  spec.aot = true
  spec.aot_include_widened = true

  if build.picoruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-pack"
  elsif build.femtoruby?
    spec.add_dependency 'picoruby-pack'
  end
end
