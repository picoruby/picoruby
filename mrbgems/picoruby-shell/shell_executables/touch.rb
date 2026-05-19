now = Time.now
path = Shell::ARGV[0]
if File.exist? path
  File.utime(now, now, path)
else
  File.open(path, "w").close
end
