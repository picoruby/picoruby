require "shell"
require "sqlite3"
require "picorubyvm"

shell = Shell.new
shell.setup_root_volume(:ram)
shell.setup_system_files

Dir.open "/" do |dir|
  while ent = dir.read
    p ent
  end
end

db = SQLite3::Database.new "/home/test.db"

stmt = db.prepare "CREATE TABLE IF NOT EXISTS test
  (
    id INTEGER PRIMARY KEY,
    name TEXT,
    created_at TEXT DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')),
    updated_at TEXT DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW'))
  );"
stmt.execute
puts PicoRubyVM.memory_statistics

print "journal_mode="
db.execute("PRAGMA journal_mode;") do |row|
  print row[0]
end

stmt = db.prepare "INSERT INTO test (name) VALUES (?);"

%w{Mario Luigi Peach Toad Koopa Kuribo a n d l iel oa g}.each do |name|
  puts name
  stmt.execute name
  puts PicoRubyVM.memory_statistics
end

##db.execute("SELECT datetime('now', '+9 hours');") do |row|
#resultset = db.execute("SELECT STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW');")
#resultset.each do |row|
#  p row
#end
#
#db.execute("SELECT STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW');") do |row|
#  p row
#end
#
#db.execute("SELECT * FROM test;") do |row|
#  p row
#end
#
#db.results_as_hash = true
#
#db.execute("SELECT * FROM test;") do |row|
#  p row
#end
#
db.close
#p File::Stat.new("/home/test.db").size
#File.open("/home/test.db") do |f|
#  p f.read
#end

#db = SQLite3::Database.new "/home/test.db"
#db.execute "INSERT INTO test (name) VALUES ('Principessa Pesca');"
#db.execute "SELECT * FROM test;" do |row|
#  p row
#end
#db.close

#now = Time.now
#p now.to_s
#p now.inspect
#p now.to_i
#p now.to_f - now.to_i
#p now.usec
