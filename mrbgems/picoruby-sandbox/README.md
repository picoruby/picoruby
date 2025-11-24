# picoruby-sandbox

Sandboxed execution environment for PicoRuby - safe code evaluation.

## Overview

Provides isolated execution environment for running untrusted Ruby code with restricted access.

## Usage

```ruby
require 'sandbox'

# Create sandbox
sandbox = Sandbox.new

# Evaluate code in sandbox
result = sandbox.eval("1 + 2")
puts result  # => 3

# Code runs in isolated environment
sandbox.eval("@var = 10")
sandbox.eval("puts @var")  # => 10

# Outside scope is unaffected
puts defined?(@var)  # => nil
```

## API

### Methods

- `Sandbox.new()` - Create new sandbox
- `eval(code)` - Evaluate Ruby code in sandbox

## Security Features

- Isolated variable scope
- Restricted access to dangerous operations
- Prevents access to host environment
- Safe for executing user-provided code

## Use Cases

- Running user scripts safely
- Plugin systems
- Code playgrounds
- Educational platforms
- Dynamic code execution

## Notes

- Not a complete security solution
- Use in combination with other security measures
- Limit execution time to prevent infinite loops
- Monitor resource usage
- May have limitations compared to full Ruby sandbox solutions
