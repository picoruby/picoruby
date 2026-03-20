MRUBYC_SIG = "mrbgems/picoruby-mrubyc/sig"

target :mrbgems do
#  stdlib_path(
#    core_root:   MRUBYC_SIG,
#    stdlib_root: "" # Skip loading stdlib RBSs
#  )

  Dir.glob("**/sig/").each do |dir|
    # Exclude vendor/ because gems installed there (e.g. on GHA) have sig/ dirs
    # that cause duplicate RBS declaration errors.
    unless dir.include?("lib/prism") || dir.include?("build/repos") || dir.include?(MRUBYC_SIG) || dir.include?("task-ext") || dir.include?("-prk-") || dir.include?("vendor/")
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
  ignore "mrbgems/picoruby-net/mrblib/ntp.rb"
  ignore "mrbgems/picoruby-picotest/mrblib/picotest/runner.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/array.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/range.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/string.rb"
  ignore "mrbgems/picoruby-task-ext/mrblib/task.rb"
  ignore "mrbgems/picoruby-mruby/lib/mruby"
  ignore "mrblib"
  ignore "build"
  ignore "mrbgems/picoruby-prk-*/mrblib/*.rb"

  # TODO: Fix after removal of picoruby-net
  ignore "mrbgems/picoruby-shell/shell_executables/wifi_connect.rb"
  # Task class sig is excluded from signature loading (task-ext)
  ignore "mrbgems/picoruby-shell/shell_executables/taskstat.rb"
  # R2P2 uses IO.new without arguments (embedded environment)
  ignore "mrbgems/picoruby-r2p2/mrblib/main_task.rb"

  # mrubyc submodule files that cannot be modified
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/object.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/global.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/numeric.rb"
  ignore "mrbgems/picoruby-mrubyc/lib/mrubyc/mrblib/hash.rb"
end
