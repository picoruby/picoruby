# picoruby-numeric-ext

Extended numeric methods for PicoRuby (Float rounding methods).

## Usage

```ruby
require 'numeric-ext'

# Floor - round down
puts 3.7.floor      # => 3
puts (-3.7).floor   # => -4
puts 3.14159.floor(2)  # => 3.14

# Ceil - round up
puts 3.2.ceil       # => 4
puts (-3.2).ceil    # => -3
puts 3.14159.ceil(2)   # => 3.15

# Round - round to nearest
puts 3.5.round      # => 4
puts 3.4.round      # => 3
puts (-3.5).round   # => -4
puts 3.14159.round(2)  # => 3.14
puts 3.14159.round(3)  # => 3.142
```

## API

### Float Methods

- `floor()` - Round down to Integer
- `floor(digits)` - Round down to specified decimal places
- `ceil()` - Round up to Integer
- `ceil(digits)` - Round up to specified decimal places
- `round()` - Round to nearest Integer
- `round(digits)` - Round to specified decimal places

## Return Values

- **No argument**: Returns Integer
- **With digits argument**:
  - Returns Integer if digits is 0
  - Returns Float if digits > 0

## Notes

- Extends Float class with CRuby-compatible rounding methods
- Negative values round "away from zero" for floor/ceil
- `round` uses "round half up" strategy (0.5 rounds to 1)
