# Create demo users
alice = User.find_or_create_by!(username: "alice") do |u|
  u.password = "password"
  u.display_name = "Alice Johnson"
end

bob = User.find_or_create_by!(username: "bob") do |u|
  u.password = "password"
  u.display_name = "Bob Smith"
end

charlie = User.find_or_create_by!(username: "charlie") do |u|
  u.password = "password"
  u.display_name = "Charlie Brown"
end

# Create demo channels
general = Channel.find_or_create_by!(name: "general") do |c|
  c.description = "General discussion for everyone"
end

random = Channel.find_or_create_by!(name: "random") do |c|
  c.description = "Random chat and off-topic discussions"
end

tech = Channel.find_or_create_by!(name: "tech") do |c|
  c.description = "Technology and programming discussions"
end

# Create some sample messages
if Message.count == 0
  Message.create!([
    { user: alice, channel: general, content: "Hello everyone! Welcome to the chat!" },
    { user: bob, channel: general, content: "Hi Alice! Great to be here." },
    { user: charlie, channel: general, content: "Hey folks! This is awesome!" },
    { user: alice, channel: tech, content: "Anyone working on interesting projects?" },
    { user: bob, channel: tech, content: "Im building a chat app with Rails and Funicular!" },
    { user: charlie, channel: tech, content: "That sounds cool! How is it going?" },
    { user: bob, channel: tech, content: "Pretty well! ActionCable is really powerful." },
    { user: alice, channel: random, content: "Whats everyone having for lunch?" },
    { user: charlie, channel: random, content: "Pizza! Always pizza. :)" }
  ])
end

puts "Seed data created successfully!"
puts "Users: #{User.count}"
puts "Channels: #{Channel.count}"
puts "Messages: #{Message.count}"
