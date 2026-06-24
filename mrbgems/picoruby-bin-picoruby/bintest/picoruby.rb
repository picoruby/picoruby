require 'open3'

PICORUBY_BIN = 'picoruby'

assert('picoruby reports an unhandled exception') do
  out, err, status = Open3.capture3(cmd_bin(PICORUBY_BIN), '-e', 'raise "boom"')

  assert_equal('', out)
  assert_match('*boom (RuntimeError)*', err)
  assert_false(status.success?)
end
