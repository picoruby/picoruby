load File.join(File.dirname(__FILE__), "r2p2_config.rb")

MRuby::Gem::Specification.new('picoruby-r2p2') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'R2P2 firmware for Raspberry Pi Pico'
end
