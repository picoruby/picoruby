# PicoRuby Testing Guide

This document outlines the testing framework used in PicoRuby, explaining how to run tests and the different strategies involved.

## Running Tests

The primary way to run tests is through Rake tasks.

### Running All Tests

To run all tests for all gems, use the following command:

```bash
rake test:all
```

This command will execute the following tasks:
-   `compiler:picoruby`
-   `compiler:microruby`
-   `gems:steep`
-   `gems:microruby`
-   `gems:picoruby`

### Running Tests for a Specific Gem

You can run tests for a specific gem by providing the `specified_gem` argument to the `test:gems:picoruby` or `test:gems:microruby` Rake tasks.

For PicoRuby:
```bash
rake 'test:gems:picoruby[picoruby-some-gem]'
```

For MicroRuby:
```bash
rake 'test:gems:microruby[picoruby-some-gem]'
```

## Testing Strategies

The testing framework employs two main strategies to efficiently test the different types of gems in the project.

### Strategy 1: Full Build

This strategy is used for gems that have C extensions but are not platform-dependent. These gems are identified by the presence of a `src` directory and the absence of a `ports` directory.

For each of these gems, a dedicated test binary is built. This ensures that the C extensions are correctly compiled and linked with the test runner. While this approach is robust, it is also slower due to the per-gem compilation.

### Strategy 2: Dynamic Require / Mocking

This strategy is used for two types of gems:
1.  **Pure Ruby gems:** Gems that do not have a `src` directory.
2.  **Platform-dependent gems:** Gems that have a `ports` directory.

For these gems, a single, generic test runner is built for each VM (PicoRuby and MicroRuby). The test runner then dynamically loads the gem's Ruby files (`mrblib/**/*.rb`). If a `test/mock` directory exists, its contents are also loaded, which is useful for mocking hardware-dependent C extensions when testing on the host.

This strategy is significantly faster because it avoids rebuilding the test runner for each gem.

## Test Implementation

Tests are written using the `picoruby-picotest` gem. Test files should be placed in the `test` directory of the gem. Test classes must end with the `Test` suffix for the runner to discover them.

Example of a test file:
```ruby
# mrbgems/picoruby-some-gem/test/some_test.rb

class SomeTest < Picotest::Test
  def test_something
    assert_equal(1, 1)
  end
end
```

## Test Runner

The test runner is implemented in `mrbgems/picoruby-picotest/mrblib/picotest/runner.rb`. It is responsible for loading test files, running the tests, and summarizing the results.

Note that the test runner itself runs in a CRuby process, while the dispatched test runs in a PicoRuby or MicroRuby process.

A key feature of the runner is that it cleans up defined test classes after each run. This is done in an `ensure` block to prevent test definitions from leaking between different test files, which ensures test isolation.
