target :lib do
  library "io-console"

  signature "sig/prk_firmware"
  Dir.glob("mrbgems/**/sig/").each do |dir|
    signature dir
  end

  check "mrblib"
  Dir.glob([
    "mrbgems/**/mrblib/",
    "mrbgems/**/task/"
  ]).each { |dir| check dir }

  ignore "mrbgems/picoruby-mrubyc/repos/mrubyc/mrblib/*.rb"
  ignore "mrbgems/picoruby-vfs/mrblib/file.rb"
  ignore "mrbgems/picoruby-vfs/mrblib/dir.rb"
  ignore "mrbgems/picoruby-shell/mrblib/0_out_of_steep.rb"
end
