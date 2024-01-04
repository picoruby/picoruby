target :mrbgems do
  stdlib_path(
    core_root:   false,
    stdlib_root: false # Skip loading stdlib RBSs
  )

  signature "sig/prk_firmware"
  Dir.glob("**/sig/").each do |dir|
    signature dir
  end

  check "mrblib"
  Dir.glob([
    "**/mrblib/",
    "**/task/"
  ]).each { |dir| check dir }

  # Skip checking String as #each_char and #each_byte raise error
  ignore "mrbgems/picoruby-mrubyc/repos/mrubyc/mrblib/array.rb"
  ignore "mrbgems/picoruby-mrubyc/repos/mrubyc/mrblib/range.rb"
  ignore "mrbgems/picoruby-mrubyc/repos/mrubyc/mrblib/string.rb"
end
