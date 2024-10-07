MRUBYC_SIG = "mrbgems/picoruby-mrubyc/sig"

target :mrbgems do
  stdlib_path(
    core_root:   MRUBYC_SIG,
    stdlib_root: "" # Skip loading stdlib RBSs
  )

  signature "sig/prk_firmware"
  Dir.glob("**/sig/").each do |dir|
    unless dir.include?("lib/prism/sig") || dir.include?(MRUBYC_SIG) || dir.include?("task-ext")
      signature dir
    end
  end

  check "mrblib"
  Dir.glob([
    "**/mrblib/",
    "mrbgems/picoruby-shell/shell_executables/",
    "**/task/"
  ]).each { |dir| check dir }

  # Skip checking String as #each_char and #each_byte raise error
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/array.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/range.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/string.rb"
  ignore "mrbgems/picoruby-task-ext/mrblib/task.rb"
end
