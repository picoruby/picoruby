# picoruby-eval

Dynamic code evaluation for PicoRuby.

## Usage

```ruby
require 'eval'

# Evaluate Ruby code from string
result = eval("1 + 2")
puts result  # => 3

# Evaluate complex expressions
code = <<~RUBY
  x = 10
  y = 20
  x + y
RUBY
result = eval(code)
puts result  # => 30

# Access variables from outer scope
a = 5
result = eval("a * 2")
puts result  # => 10
```

## API

### Methods

- `eval(code)` - Evaluate Ruby code string and return result

## Notes

- Code is parsed and executed at runtime
- Has access to variables in the current scope
- Slower than compiled code
- Use with caution - evaluating untrusted input is a security risk
- Syntax errors in eval'd code will raise exceptions
