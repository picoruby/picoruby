# The runner also `load`s this file on CRuby to find the test classes;
# "littlefs" does not exist there, and LoadError is not a StandardError.
begin
  require "littlefs"
rescue LoadError
end

# Vim drives an Editor::Screen, which asks the terminal for its size when
# it is made; TERM=dumb makes it take 24x80 without a terminal. Files go
# through picoruby-vfs, so a RAM littlefs is mounted at "/" (once per
# process; the volume outlives a test method, so every test uses its own
# file names). The key loop (_start) needs a terminal and stays untested;
# the command line and the file round trip are driven directly.
class VimTest < Picotest::Test
  def setup
    skip "needs instance_variable_get (mruby-metaprog)" if femtoruby?
    ENV["TERM"] = "dumb"
    unless VFS::VOLUMES.any? { |v| v[:mountpoint] == "/" }
      fs = Littlefs.new(:flash, label: "VIMTEST")
      fs.mkfs
      VFS.mount(fs, "/")
    end
    ENV["PWD"] = "/"
  end

  def fresh(path)
    File.unlink(path) if File.exist?(path)
    path
  end

  def write_file(path, content)
    fresh(path)
    File.open(path, "w") { |f| f.write(content) }
    path
  end

  # The command typed after ":" and the text buffer of a Vim instance.
  def command(vim, line)
    vim.instance_variable_get(:@command_buffer).lines[0] = line
  end

  def text_buffer(vim)
    vim.instance_variable_get(:@editor).instance_variable_get(:@buffer)
  end

  def run_command(vim, line)
    command(vim, line)
    vim.exec_command(text_buffer(vim))
  end

  # --- :q ---

  def test_quit_with_an_unchanged_buffer
    vim = Vim.new("")
    assert_equal(:quit, run_command(vim, ":q"))
  end

  def test_quit_refuses_an_unsaved_change
    vim = Vim.new("")
    text_buffer(vim).put("x")
    assert_equal("No write since last change", run_command(vim, ":q"))
    assert_equal(:quit, run_command(vim, ":q!"))
  end

  # --- :w ---

  def test_write_without_a_file_name
    vim = Vim.new("")
    assert_equal("No file name", run_command(vim, ":w"))
  end

  def test_write_to_a_named_file
    path = fresh("/vim_named.txt")
    vim = Vim.new("")
    buffer = text_buffer(vim)
    buffer.put("h")
    buffer.put("i")
    run_command(vim, ":w #{path}")
    assert(File.exist?(path))
    assert_equal("hi", File.load_file(path).to_s.chomp)
  end

  def test_write_and_quit
    path = fresh("/vim_wq.txt")
    vim = Vim.new(path)
    text_buffer(vim).put("z")
    assert_equal(:quit, run_command(vim, ":wq"))
    assert_equal("z", File.load_file(path).to_s.chomp)
  end

  # --- opening files ---

  def test_an_existing_file_is_loaded_into_the_buffer
    path = write_file("/vim_in.txt", "alpha\nbeta\n")
    vim = Vim.new(path)
    lines = text_buffer(vim).lines
    assert_equal("alpha", lines[0])
    assert_equal("beta", lines[1])
  end

  def test_a_new_file_is_announced
    path = fresh("/vim_new.txt")
    vim = Vim.new(path)
    assert_equal("\"#{path}\" [New]", vim.instance_variable_get(:@message))
    assert_false(File.exist?(path))
  end

  def test_a_relative_path_expands_against_pwd
    Dir.mkdir("/vimdir") unless Dir.exist?("/vimdir")
    Dir.chdir("/vimdir")
    vim = Vim.new("rel.txt")
    assert_equal("/vimdir/rel.txt", vim.instance_variable_get(:@filepath))
    Dir.chdir("/")
  end

  # --- unknown commands ---

  def test_an_unknown_command_is_reported
    vim = Vim.new("")
    assert_equal("Not an editor command: :foo", run_command(vim, ":foo"))
    assert_equal("Not an editor command: :w a b", run_command(vim, ":w a b"))
    assert_equal("Not an editor command: :x", run_command(vim, ":x"))
  end

  def test_clear_command_buffer
    vim = Vim.new("")
    command(vim, ":q")
    vim.clear_command_buffer
    assert_equal("", vim.instance_variable_get(:@command_buffer).lines[0])
  end
end
