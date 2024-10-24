## Usage

```ruby
require "picotest"
Picotest::Runner.run("fullpath/to/test/directory")
```

### Filtering by filename

```ruby
require "picotest"
Picotest::Runner.run("fullpath/to/test/directory", "specific_test")
```

This tests only the files that include `specific_test`.
