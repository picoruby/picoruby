now = Time.now
path = $*[0]
if File.exist? path
  File.utime(now, now, path)
else
  File.open(path, "w").close
end
