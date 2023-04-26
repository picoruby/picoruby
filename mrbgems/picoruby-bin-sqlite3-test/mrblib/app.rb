require "shell"
require "sqlite3"

shell = Shell.new
shell.setup_root_volume(:ram)
shell.setup_system_files

Dir.open "/" do |dir|
  while ent = dir.read
    p ent
  end
end

db = SQLite3::Database.new "/home/test.db"

#db.execute("SELECT CURRENT_TIME;") do |row|
#  p row
#end

#  db.execute "CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT);"
#db.execute "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT);"
stmt = db.prepare "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT);"
stmt.execute
stmt = db.prepare "INSERT INTO test (name) VALUES (?);"
stmt.execute "Mario"
stmt.execute "Luigi"
stmt.execute "Kuppa"
stmt.execute "Principessa Pesca"


#stmt.bind_params "Kuppa"
#puts 4
#stmt.execute

db.execute("SELECT * FROM test;") do |row|
  p row
end

db.close
p File::Stat.new("/home/test.db").size
#File.open("/home/test.db") do |f|
#  p f.read
#end

#db = SQLite3::Database.new "/home/test.db"
#db.execute "INSERT INTO test (name) VALUES ('Principessa Pesca');"
#db.execute "SELECT * FROM test;" do |row|
#  p row
#end
#db.close

