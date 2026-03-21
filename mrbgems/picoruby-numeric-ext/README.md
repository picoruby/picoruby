# picoruby-numeric-ext

Extended numeric methods for PicoRuby (Float rounding and modulo).

## Usage

```ruby
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

# Modulo
puts 7.0 % 3.0      # => 1.0
puts (-7.0) % 3.0   # => 2.0
puts 7.0 % (-3.0)   # => -2.0
```

## API

### Float Methods

- `floor` - Round down to Integer
- `floor(digits)` - Round down to specified decimal places
- `ceil` - Round up to Integer
- `ceil(digits)` - Round up to specified decimal places
- `round` - Round to nearest Integer
- `round(digits)` - Round to specified decimal places
- `%` - Modulo (result has the same sign as the divisor)

## Return Values

- **Rounding methods**:
  - Returns Integer if digits <= 0 (default)
  - Returns Float if digits > 0
- **Modulo**: Always returns Float

## Notes

- Extends Float class with CRuby-compatible rounding methods and modulo
- `round` uses "round half away from zero" strategy (0.5 rounds to 1, -0.5 rounds to -1)
- `%` raises ZeroDivisionError when the divisor is zero
