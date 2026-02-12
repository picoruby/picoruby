# Picotest Architecture

## Overview

Picotest is a test framework that runs test code on **target VMs** (PicoRuby / MicroRuby) while orchestrating the process from **CRuby**.
This two-process architecture is key to understanding how the files relate.

## Test Execution Flow

```
CRuby (Rake)                          Target VM (PicoRuby / MicroRuby)
============                          ==================================

1. test.rake
   |
   v
2. run_test_for_gems()
   - Builds target VM binary
   - Calls Picotest::Runner
   |
   v
3. Runner#run_test(entry)
   - `load entry` in CRuby
     to discover test classes
   - Calls Test#list_tests
     to enumerate test_* methods
   - Generates a temp .rb script
   |                                  4. Target VM executes temp script:
   |  `picoruby tmpfile.rb`  -------->   - require 'picotest' (pre-built gem)
   |                                     - load test file
   |                                     - Run each test_* method
   |                                     - Print results as JSON
   |
5. Runner parses stdout    <--------  (stdout with JSON after separator)
   - Collects results
   - Summarizes pass/fail
```

## File Roles

### `picotest.rb` — Entry point (both CRuby and target VM)

Conditionally requires files based on `RUBY_ENGINE`:
- CRuby (`"ruby"`): requires `test.rb` and `runner.rb`
- MicroRuby (`"mruby"`): only `test.rb` and `double.rb` (loaded as gem)
- PicoRuby (`"mruby/c"`): only `test.rb` and `double.rb` (loaded as gem)

### `test.rb` — Test base class (both CRuby and target VM)

- **On CRuby**: Loaded so that Runner can instantiate test classes and call
  `list_tests` to discover `test_*` methods. Assertions are NOT exercised here.
- **On target VM**: Actually runs assertions (`assert_equal`, `assert_raise`, etc.)
  and collects results into `@result` hash, which is serialized as JSON.

### `runner.rb` — Test orchestrator (CRuby only)

- Discovers test files (`*_test.rb`) in gem test directories
- Loads each test file in CRuby to discover test classes
- Generates a temporary Ruby script for the target VM
- Spawns the target VM as a subprocess (`IO.popen`)
- Parses JSON output and prints summary

### `double.rb` — Stub/mock support (target VM only)

- Provides `stub()` and `mock()` functionality
- Only meaningful at runtime on the target VM
- Inherits from `BasicObject` to intercept all method calls via `method_missing`

## Gem Test Directory Structure

```
mrbgems/picoruby-<gem>/
  test/
    *_test.rb          # Test files (loaded by both CRuby and target VM)
    target_vm          # Optional file specifying "picoruby" or "microruby"
```

## Key Environment Variables

| Variable | Description |
|---|---|
| `RUBY` | Path to target VM binary |
| `PICORUBY_TEST_TARGET_VM` | Set by test.rake to `./build/host/bin/{vm_type}` |
| `SKIP_BUILD` | Skip building the target binary |
| `PICORUBY_DEBUG` | Enable debug build |
| `MRUBY_CONFIG` | Build config path |
