MRuby::Gem::Specification.new('picoruby-ota') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'OTA update manager for R2P2'

  spec.add_dependency 'picoruby-env'
  spec.add_dependency 'picoruby-yaml'
  spec.add_dependency 'picoruby-crc'

  spec.test_rbfiles = Dir.glob("#{spec.dir}/test/*.rb")

  # Embed ECDSA public key for signature verification (optional)
  pem_file = "#{dir}/keys/ota_ecdsa_public.pem"
  if File.exist?(pem_file)
    spec.cc.defines << "OTA_HAS_ECDSA_KEY"
    spec.cc.include_paths << build_dir

    key_inc = "#{build_dir}/ota_public_key.c.inc"
    directory build_dir
    file key_inc => [pem_file, build_dir] do |t|
      pem = File.read(pem_file).strip
      File.open(t.name, 'w') do |f|
        f.puts "static const char ota_ecdsa_public_key_pem[] ="
        pem.each_line do |line|
          f.puts "  \"#{line.chomp}\\n\""
        end
        f.puts ";"
      end
    end
    Rake::Task[key_inc].invoke
  end
end
