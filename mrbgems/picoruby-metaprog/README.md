# picoruby-metaprog

Metaprogramming utilities for PicoRuby - reflection and dynamic programming.

## Usage

```ruby
require 'metaprog'

# Send dynamic method calls
obj.send(:method_name, arg1, arg2)
obj.send("method_name", arg1, arg2)

# Introspection
methods = obj.methods
puts methods.inspect  # => [:method1, :method2, ...]

# Instance variables
vars = obj.instance_variables
puts vars.inspect  # => [:@var1, :@var2, ...]

# Get/set instance variables
value = obj.instance_variable_get(:@var_name)
obj.instance_variable_set(:@var_name, "new value")

# Check if method exists
if obj.respond_to?(:method_name)
  obj.method_name
end

# Object ID
id = obj.__id__  # or obj.object_id
puts "Object ID: #{id}"

# Type checking
if obj.instance_of?(String)
  puts "obj is a String"
end

# Class introspection
if obj.class?
  puts "obj is a Class"
end

ancestors = MyClass.ancestors
puts ancestors.inspect

# Constant access
value = MyClass.const_get(:CONSTANT_NAME)

# Instance eval - evaluate block in object context
obj.instance_eval do
  @internal_var = "modified"
end

# Alias methods
class MyClass
  def old_name
    "hello"
  end
  alias_method :new_name, :old_name
end

# Call stack
stack = caller
stack = caller(0, 5)  # First 5 frames

# Ruby executable path (compatibility)
ruby_path = RbConfig.ruby
```

## API

### Object Methods

- `send(name, *args)` - Call method dynamically
- `methods()` - Get array of method names
- `instance_variables()` - Get array of instance variable names
- `instance_variable_get(name)` - Get instance variable value
- `instance_variable_set(name, value)` - Set instance variable value
- `respond_to?(name)` - Check if method exists
- `__id__()` / `object_id()` - Get object ID
- `instance_of?(klass)` - Check if object is instance of class
- `const_get(name)` - Get constant value
- `class?()` - Check if object is a Class
- `ancestors()` - Get ancestor chain
- `instance_eval { }` - Evaluate block in object's context
- `alias_method(new_name, old_name)` - Create method alias

### Kernel Methods

- `caller(start = 0, length = nil)` - Get call stack

### RbConfig

- `RbConfig.ruby()` - Get Ruby executable path

## Notes

- Enables runtime introspection and modification
- Useful for DSLs, frameworks, and dynamic behavior
- `send` can call private methods
- Use with caution - powerful but can reduce code clarity
