# The runner also `load`s this file on CRuby to find the test classes;
# "littlefs" does not exist there, and LoadError is not a StandardError.
begin
  require "littlefs"
rescue LoadError
end

# picoruby-vfs routes File and Dir to a mounted volume. The tests mount a
# RAM littlefs (ports/posix) at "/" once per process; the volume outlives a
# test method, so every test uses its own file names and removes what it
# makes first.
class VFSTest < Picotest::Test
  def setup
    unless VFS::VOLUMES.any? { |v| v[:mountpoint] == "/" }
      # Format before mounting: an unformatted device makes littlefs print
      # a "Corrupted dir pair" trace to stdout, which corrupts the result
      # JSON the runner parses.
      fs = Littlefs.new(:flash, label: "VFSTEST")
      fs.mkfs
      VFS.mount(fs, "/")
    end
    # The host's PWD must not leak into relative paths.
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

  # --- pure path functions ---

  def test_sanitize_removes_dot_and_dotdot
    assert_equal("/a/c", VFS.sanitize("/a/./b/../c"))
    assert_equal("/", VFS.sanitize("/"))
    assert_equal("/a/b", VFS.sanitize("//a///b/"))
  end

  def test_sanitize_resolves_a_relative_path_against_pwd
    ENV["PWD"] = "/work"
    assert_equal("/work/x/y", VFS.sanitize("x/y"))
    assert_equal("/x", VFS.sanitize("../x"))
    ENV["PWD"] = "/"
  end

  def test_expand_path
    assert_equal("/a/b", File.expand_path("b", "/a"))
    assert_equal("/x", File.expand_path("/x", "/a"))
    assert_equal("/a", File.expand_path("../a", "/b"))
  end

  def test_dirname
    assert_equal("/a/b", File.dirname("/a/b/c"))
    assert_equal("/", File.dirname("/a"))
    assert_equal(".", File.dirname("a"))
    assert_equal("/a", File.dirname("/a/b/c", 2))
    assert_equal("/a", File.dirname("/a/b//"))
    assert_equal("/a/b/c", File.dirname("/a/b/c", 0))
    assert_raise(ArgumentError) { File.dirname("/a", -1) }
  end

  # --- files ---

  def test_write_then_read_back
    path = fresh("/rw.txt")
    File.open(path, "w") do |f|
      f.write("hello")
      f.puts("world")
    end
    assert(File.exist?(path))
    assert(File.file?(path))
    assert_false(File.directory?(path))
    assert_equal("helloworld\n", File.load_file(path))
    File.open(path) { |f| assert_equal(11, f.size) }
  end

  def test_write_returns_the_byte_count_and_takes_several_arguments
    path = fresh("/count.txt")
    n = File.open(path, "w") { |f| f.write("ab", "cde", 7) }
    assert_equal(6, n)
    assert_equal("abcde7", File.load_file(path))
  end

  def test_gets_across_the_read_chunk
    long = "a" * 200
    path = write_file("/lines.txt", "#{long}\nbb\n")
    File.open(path) do |f|
      assert_equal("#{long}\n", f.gets)
      assert_equal("bb\n", f.gets)
      assert_nil(f.gets)
      assert(f.eof?)
    end
  end

  def test_gets_with_chomp_and_limit
    path = write_file("/gets.txt", "first\nsecond\n")
    File.open(path) do |f|
      assert_equal("first", f.gets(chomp: true))
      assert_equal("sec", f.gets(3))
      assert_equal("ond\n", f.gets)
    end
  end

  def test_each_line
    path = write_file("/each.txt", "1\n2\n3\n")
    seen = []
    File.open(path) { |f| f.each_line { |l| seen << l } }
    assert_equal(["1\n", "2\n", "3\n"], seen)
  end

  def test_seek_tell_rewind_read_getbyte
    path = write_file("/seek.txt", "0123456789")
    File.open(path) do |f|
      assert_equal("012", f.read(3))
      assert_equal(3, f.tell)
      f.seek(5)
      assert_equal("56", f.read(2))
      f.rewind
      assert_equal(48, f.getbyte)
      assert_equal("", f.read(0))
      assert_equal("123456789", f.read)
      assert_nil(f.read(1))
      assert_raise(ArgumentError) { f.read(-1) }
    end
  end

  def test_putc_and_printf
    path = fresh("/putc.txt")
    File.open(path, "w") do |f|
      f.putc("xyz")
      f.putc(65)
      f.printf("%d-%s", 7, "q")
    end
    assert_equal("xA7-q\n", File.load_file(path))
  end

  def test_load_file_with_length_and_offset
    path = write_file("/slice.txt", "abcdefgh")
    assert_equal("cde", File.load_file(path, 3, 2))
    assert_equal("abcdefgh", File.load_file(path))
  end

  def test_rename_and_unlink
    from = write_file("/from.txt", "x")
    to = fresh("/to.txt")
    File.rename(from, to)
    assert_false(File.exist?(from))
    assert(File.exist?(to))
    assert_equal(1, File.unlink(to))
    assert_false(File.exist?(to))
  end

  def test_stat
    path = write_file("/stat.txt", "12345")
    st = File::Stat.new(path)
    assert_equal(5, st.size)
    assert_false(st.directory?)
    Dir.mkdir("/statdir") unless Dir.exist?("/statdir")
    assert(File::Stat.new("/statdir").directory?)
  end

  # --- directories ---

  def test_mkdir_exist_empty_and_entries
    Dir.mkdir("/d1") unless Dir.exist?("/d1")
    assert(Dir.exist?("/d1"))
    assert(File.directory?("/d1"))
    File.unlink("/d1/inner.txt") if File.exist?("/d1/inner.txt")
    assert(Dir.empty?("/d1"))
    write_file("/d1/inner.txt", "i")
    assert_false(Dir.empty?("/d1"))
    names = []
    Dir.open("/d1") { |d| d.each { |n| names << n } }
    assert(names.include?("inner.txt"))
  end

  def test_dir_read_and_rewind
    Dir.mkdir("/d2") unless Dir.exist?("/d2")
    write_file("/d2/one.txt", "1")
    d = Dir.open("/d2")
    assert_equal("/d2", d.path)
    first = []
    while (n = d.read)
      first << n
    end
    d.rewind
    again = []
    while (n = d.read)
      again << n
    end
    d.close
    assert_equal(first, again)
    assert(first.include?("one.txt"))
  end

  def test_chdir_and_pwd
    Dir.mkdir("/d3") unless Dir.exist?("/d3")
    write_file("/d3/here.txt", "h")
    Dir.chdir("/d3")
    assert_equal("/d3", Dir.pwd)
    assert(File.exist?("here.txt"))
    assert_equal("/d3/here.txt", File.expand_path("here.txt", Dir.pwd))
    Dir.chdir("/")
    assert_equal("/", Dir.pwd)
  end

  def test_glob_in_pwd
    write_file("/glob_a.txt", "a")
    write_file("/glob_b.log", "b")
    ENV["PWD"] = "/"
    found = Dir.glob("glob_*.txt")
    assert(found.include?("glob_a.txt"))
    assert_false(found.include?("glob_b.log"))
  end

  # --- the volume table ---

  def test_split_picks_the_longest_mountpoint
    volume, path = VFS.split("/some/where")
    assert_equal("/", volume[:mountpoint])
    assert_equal("/some/where", path)
  end

  def test_mount_twice_at_the_same_point_raises
    assert_raise(RuntimeError) { VFS.mount(Littlefs.new(:flash, label: "DUP"), "/") }
  end

  def test_mountpoint_must_be_absolute
    assert_raise(ArgumentError) { VFS.mount(Littlefs.new(:flash, label: "REL"), "rel") }
  end
end
