# picoruby-aot

picoruby-aot compiles typed methods of a gem's `mrblib` into mruby cfuncs at
build time. The VM, the irep and irb stay as they are. A compiled method is
an ordinary C method of its class.

Spinel (the Ruby AOT compiler) runs on the host as a type oracle. It reads
the gem's Ruby and the RBS files of its dependencies, and writes a JSON file
with a type for every node. picoruby-aot reads that JSON and the Prism AST, and
writes C against the mruby API. Spinel's own C and runtime are not used.

## Use

Turn it on for a target, then opt in each gem:

```ruby
conf.aot                                     # spinel from $SPINEL or PATH
conf.aot path: "/path/to/spinel"             # or an explicit binary
conf.gem(core: 'picoruby-psg') { |g| g.aot = true }
```

A gem can keep a method in the irep:

```ruby
spec.aot_exclude = %w[PSG::Driver#debug_dump]
```

Without `conf.aot` nothing changes. FemtoRuby (mruby/c) ignores `aot`.

## What becomes a cfunc

A `def` becomes a cfunc when all of these hold:

- Spinel typed every parameter and the return value (not "widened").
- The method takes only required positional parameters.
- The body uses only the supported syntax: literals, locals, instance
  variables, constants, `if`/`unless`/`while`/`until`, `return`, `break`,
  `next`, `and`/`or`/`not`, method calls without blocks or keywords, and
  `raise` with a constant class.
- The method does not wait for the scheduler (`sleep`, `sleep_ms`,
  `Task.pass`, `join`, a Task receiver), directly or through another
  method of the gem. A cfunc cannot yield to other tasks.
- The method is not `initialize`.

Every other `def` stays in the irep. The build writes the decision and the
reason for each method to `build/<target>/mrbgems/<gem>/aot/manifest.json`.

## How the generated C behaves

- Arguments, instance variables, constants and call results stay boxed
  (`mrb_value`). Only values the C computes itself (literals, arithmetic
  on such values) are unboxed.
- Operators on boxed values go through guarded helpers (`aot_add`, ...).
  The helper computes Integer and Float cases inline and sends everything
  else to the VM, so nil, Bignum and user-defined operators behave as in
  the irep.
- Other calls go through `mrb_funcall_argv`.
- Constants resolve through the lexical scope of the `def`, then the
  receiver's ancestors, then Object.

## Twin test

The same picotest suite runs against the irep build and the AOT build:

```
rake "test:gems:picoruby[picoruby-psg]"
PICORB_AOT=1 rake "test:gems:picoruby[picoruby-psg]"
```

## Tools

- `tools/aot-analyze [--all | GEM...]`: run Spinel over gems and report how
  many defs are fully typed.
- `tools/aot-gen --gem GEM --out-c FILE --out-rb DIR [--manifest FILE]`:
  the generator the build calls. It runs Spinel itself unless `--types` is
  given.
- `tools/picoruby_aot/`: gem graph, RBS-to-Ruby stubs for C-implemented
  methods, Spinel driver, type map, code generator.

## Layout

```
include/picoruby_aot.h   helpers the generated C calls
src/picoruby_aot.c       their implementation (linked into libmruby)
tools/                host-side generator (Ruby, needs the prism and rbs gems)
```
