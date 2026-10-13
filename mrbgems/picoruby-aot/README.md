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
conf.aot                                     # the Spinel submodule, built in place
conf.aot path: "/path/to/spinel"             # an explicit binary
conf.gem(core: 'picoruby-psg') { |g| g.aot = true }
```

Spinel is the submodule `lib/spinel` of this gem, pinned to the commit
the twin tests passed with and taken as is (a change goes upstream
first, then the submodule moves). Initialize it once:

```
git submodule update --init --depth 1 mrbgems/picoruby-aot/lib/spinel
```

The first build runs `make deps` there, which fetches the vendored
parsers (prism, rbs) from rubygems.org once, then `make`. A later build
rebuilds Spinel only when the submodule's checkout moved, and then
regenerates the gems. A build without `conf.aot` never touches the
submodule. `SPINEL=/path/to/spinel` in the environment overrides the
submodule, for a developer who builds Spinel from another checkout.
`path:` overrides both.

The generator keys on the node spans, kinds and signatures that
`--emit-types` writes since Spinel's emit-types contract (#4522, #4557),
and reads the `nullable` fields (matz/spinel#8637). An older Spinel
builds and runs, but every def then stays in the irep, and the build
says so.

A gem can keep a method in the irep:

```ruby
spec.aot_exclude = %w[PSG::Driver#debug_dump]
```

A gem can also compile the methods Spinel widened to untyped. Their
values stay boxed and go through the guarded helpers, so the gain is the
VM's decode and dispatch and the direct calls between compiled methods:

```ruby
conf.gem(core: 'picoruby-json') { |g| g.aot = true; g.aot_include_widened = true }
```

Without `conf.aot` nothing changes. FemtoRuby (mruby/c) ignores `aot`.

## What becomes a cfunc

A `def` becomes a cfunc when all of these hold:

- Spinel typed every parameter and the return value (not "widened"),
  unless `aot_include_widened` is set.
- The method takes only positional parameters, required or optional.
- The body uses only the supported syntax: literals, locals, instance
  variables, constants, `if`/`unless`/`while`/`until`/`case`, `return`,
  `break`, `next`, `and`/`or`/`not`, multiple assignment to locals and
  instance variables, Range literals, method calls without blocks or
  keywords, and `raise` with a constant class.
- The method does not wait for the scheduler (`sleep`, `sleep_ms`,
  `Task.pass`, `join`, a Task receiver), directly or through another
  method of the gem. A cfunc cannot yield to other tasks.
- The method is not `initialize`.

Every other `def` stays in the irep. The build writes the decision and the
reason for each method to `build/<target>/mrbgems/<gem>/aot/manifest.json`.

## How the generated C behaves

- A value is unboxed (`mrb_int`, `mrb_float`, `mrb_bool`) when the C
  computes it itself (a literal, arithmetic on such values), or when
  Spinel proved its kind and that no nil reaches it (`"nullable":false`
  in `--emit-types`; an `Integer` and an `Integer?` share the tag `int`).
- A proved argument is read boxed and unboxed by a checked helper that
  takes the proved class and nothing else: a Float where an Integer was
  proved raises TypeError (no truncation), as does an Integer where a
  Float was proved, or anything but `true` and `false` where a bool was
  proved. An RBS that misstates a class thus shows up as TypeError at
  the method's entry, never as a reinterpreted value. The generator keeps an argument boxed
  when the body tests its class (`is_a?`, `kind_of?`, `nil?`,
  `respond_to?`, `case`): such a guard is written for a caller the
  analysis never saw, and the irep answers it with the method's own error.
- A proved instance variable read or call result is unboxed with a
  checked helper. A value the analysis did not predict (a method
  redefined at run time) raises TypeError instead of being
  reinterpreted.
- Every other argument, instance variable, constant and call result stays
  boxed (`mrb_value`).
- Operators on boxed values go through guarded helpers (`aot_add`, ...).
  The helper computes Integer and Float cases inline (and the String
  cases of `+`, `==`, `!=`, `<<`) and sends everything else to the VM,
  so nil, Bignum and user-defined operators behave as in the irep.
- A String method on a receiver Spinel typed a String goes through a
  guarded helper too (`size`, `length`, `bytesize`, `empty?`, `getbyte`,
  `[]`, `byteslice`): the String case reads the `RString` directly, any
  other receiver is sent to the VM by name. A String is not unboxed, so
  `nullable` plays no part here: the guard answers nil as the irep does.
- A string with interpolation builds a new String and appends each part
  with `mrb_str_concat`, which converts with `to_s` as the VM does.
- An Array or Hash method on a receiver Spinel typed so goes through a
  guarded helper as well. `[]` and `[]=` take the test the VM's index
  opcodes take (`mrb->idx_class`); `size`, `length`, `empty?`, `push`,
  `<<`, `pop`, `shift`, `first`, `last`, `clear`, `key?` and `delete`
  resolve the method as the VM would and compare it with the builtin
  recorded at gem init, so a redefinition on Array itself is honoured.
- Each `def` becomes two C functions: the impl, which takes `self` and the
  parameters as C arguments, and the mruby cfunc, which reads the
  arguments with `mrb_get_args` and calls the impl. A call to another
  compiled method of the gem tests, with the method lookup the VM would
  do, that the name still resolves to that cfunc, and then calls its impl
  in C with no VM frame. A redefinition, an override in a subclass or
  another receiver class takes `mrb_funcall_argv` as before. A private
  method is called directly only from an implicit or `self` receiver.
- `case/when` is an if chain. An Integer literal or an Integer Range
  pattern compares in place; a class asks `kind_of?`; the core scalars and
  a String compare as `==`; any other pattern is sent `===`.
- An optional parameter is handed to the impl as undef when the argument
  is not given, and the impl evaluates the default in method scope.
- Other calls go through `mrb_funcall_argv`.
- Constants resolve through the lexical scope of the `def` (its classes
  are recorded once, at registration), then the receiver's ancestors,
  then Object.
- The VM's registers are GC roots; the generated C's locals are not. A
  value the C holds is safe while its owner references it or while it is
  in the GC arena (every allocation, every `mrb_funcall` result). A value
  whose owner may let go of it (an instance variable read, an element
  `[]`, `first`, `shift` or `pop` hands out, a Hash `[]` or `delete`) is
  pushed to the arena at the read, which protects it until the outermost
  cfunc returns to the VM. An immediate needs nothing.
- A direct call runs on the C stack, with no mruby frame: a backtrace
  does not list it, and deep recursion through compiled methods is
  bounded by the C stack, not by mruby's.

## Twin test

The same picotest suite runs against the irep build and the AOT build:

```
rake "test:gems:picoruby[picoruby-psg]"
PICORB_AOT=1 rake "test:gems:picoruby[picoruby-psg]"
PICORB_AOT=1 PICORB_AOT_WIDENED=1 rake "test:gems:picoruby[picoruby-psg]"
```

One task runs the three and compares their summaries (the per-class and
total counts must match). Several gems share one build:

```
rake "test:gems:aot_twin[picoruby-json,picoruby-midibase-mml,picoruby-keyboard]"
```

CI runs this task for those three gems (`test-picoruby-aot` in
`.github/workflows/c-cpp.yml`). The logs land under `build/aot-twin/`.

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
