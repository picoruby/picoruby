MRuby::Gem::Specification.new('picoruby-machine') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Machine class'

  spec.add_dependency 'picoruby-require'
  spec.add_dependency 'picoruby-io-console'

  if build.posix?
    # The POSIX port runs a dedicated stdin reader thread to emulate the
    # UART/CDC RX interrupt used on bare-metal targets (see ports/posix/machine.c).
    spec.cc.flags << '-pthread'
    spec.linker.flags << '-pthread'
  end

  defines = build.cc.defines + spec.cc.defines
  use_estalloc = defines.include?("PICORB_ALLOC_ESTALLOC") &&
                 (build.picoruby? || (build.femtoruby? && defines.include?("MRBC_ALLOC_LIBC")))

  if use_estalloc
    align = defines.find { _1.start_with?("PICORB_ALLOC_ALIGN=") }.then do |define|
      define&.split("=")&.last || 4
    end
    spec.cc.defines << "ESTALLOC_ALIGNMENT=#{align}" unless spec.cc.defines.any? { _1.start_with?("ESTALLOC_ALIGNMENT=") }
    spec.cc.defines << "ESTALLOC_DEBUG=1" if defines.any? { _1.start_with?("PICORB_DEBUG") }
    spec.cc.include_paths << "#{dir}/lib/estalloc"
  end

  if build.gems.map(&:name).include?('picoruby-mruby')
    # Workaround:
    #   Locate mruby-io at the top of gem_init.c
    #   to define IO.open earlier than this gems
    if build.posix?
      spec.add_dependency 'mruby-io'
    end
  end
end
