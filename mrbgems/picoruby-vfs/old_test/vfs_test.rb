require_relative "../mrblib/vfs.rb"

class PseduoFAT
  def initialize(name)
    @prefix = "#{name}:"
  end
  def prefix
    @prefix
  end
  def mount(mountpoint)
  end
end

flash = PseduoFAT.new(:flash)
sd = PseduoFAT.new(:sd)

vfs = VFS.mount(flash, "/")
vfs = VFS.mount(sd, "/sd")

def assert(a)
  raise "Assertion failed: #{a}" unless a
end
def assert_not(a)
  raise "Assertion failed: #{a}" if a
end
def assert_equal(a, b)
  raise "Assertion failed: #{a} != #{b}" unless a == b
end

assert(VFS.volume_index("/"))
assert(VFS.volume_index("/sd"))
assert_not(VFS.volume_index("/flash"))

assert_equal "flash:", VFS::VOLUMES[0][:driver].prefix
assert_equal "sd:", VFS::VOLUMES[1][:driver].prefix

ENV["PWD"] = "/"
assert_equal "/home", VFS.sanitize_and_split("/home")[1]
assert_equal "/home", VFS.sanitize_and_split("home")[1]
assert_equal "/home", VFS.sanitize_and_split("home/")[1]

assert_equal "/sd", VFS.sanitize_and_split("/sd/home")[0][:mountpoint]
assert_equal "/home", VFS.sanitize_and_split("/sd/home")[1]
assert_equal "/home", VFS.sanitize_and_split("/sd/home/")[1]

ENV["PWD"] = "/home"
assert_equal "/home", VFS.sanitize_and_split(".")[1]

ENV["PWD"] = "/sd"
assert_equal "/home", VFS.sanitize_and_split("/home")[1]
assert_equal "/home", VFS.sanitize_and_split("/sd/home")[1]
assert_equal "/home", VFS.sanitize_and_split("home")[1]

