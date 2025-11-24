# picoruby-data

Data class for creating simple immutable value objects in PicoRuby.

## Usage

```ruby
require 'data'

# Define a Data class
Point = Data.define(:x, :y)

# Create instances
p1 = Point.new(10, 20)
p2 = Point.new(x: 5, y: 15)

# Access members
puts p1.x  # => 10
puts p1.y  # => 20

# Get all members
puts Point.members.inspect  # => [:x, :y]
puts p1.members.inspect     # => [:x, :y]

# Convert to hash
hash = p1.to_h
puts hash.inspect  # => {:x=>10, :y=>20}

# Instances are immutable (no setters)
# p1.x = 30  # Would raise error

# More complex example
Person = Data.define(:name, :age, :email)
user = Person.new("Alice", 30, "alice@example.com")
puts user.name   # => "Alice"
puts user.age    # => 30
```

## API

### Class Methods

- `Data.define(*members)` - Define new Data class with specified members
- `Data.new(*values)` - Create new instance with positional arguments
- `Data.new(**kwargs)` - Create new instance with keyword arguments
- `Data.members()` - Get array of member names (class method)

### Instance Methods

- `members()` - Get array of member names
- `to_h()` - Convert to hash with symbol keys
- Member accessor methods (one for each defined member)

## Notes

- Data classes are immutable (no setter methods)
- Members are defined as symbols
- Similar to Ruby 3.2's `Data.define`
- Lightweight alternative to Struct for simple value objects
- Arguments can be positional or keyword-based
