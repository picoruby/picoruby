# picoruby-aot architecture

This document describes picoruby-aot as it is. It says what the build
does, which defs compile, how the generated C behaves, and how the
result is verified. The plan of what comes next is `aot-plan.md`.

## 1. Pipeline

picoruby-aot compiles typed methods of a gem's `mrblib` into mruby
cfuncs at build time. The VM, the irep and irb stay as they are. A
compiled method is an ordinary C method of its class.

Spinel (the Ruby AOT compiler) runs on the host as the type oracle. It
reads the gem's Ruby and the RBS files of its dependencies and writes a
JSON file with a type for every node (`--emit-types`). picoruby-aot
reads that JSON and the Prism AST, and writes C against the mruby API.
Spinel's own C and runtime are not used.

For one gem the build does these steps:

1. The workspace prepares the input for Spinel: an entry file, the
   gem's mrblib files with their `RUBY_ENGINE == "..."` branches folded
   for mruby (Spinel answers "spinel" and would keep the mruby/c side;
   lines and columns do not move), and Ruby stubs for the C-implemented
   methods of the dependencies, generated from their RBS.
2. Spinel writes the types. The generator keys on the node spans, kinds,
   signatures and `nullable` fields of the emit-types contract. An
   older Spinel builds and runs, but every def then stays in the irep,
   and the build says so.
3. The generator classifies every def and writes one C file,
   `build/<target>/mrbgems/<gem>/aot/aot_<gem>.c`, and a copy of each
   mrblib file with the compiled defs blanked out (the line numbers do
   not move). The decision and the reason for each def go to
   `aot/manifest.json`.
4. mrbc compiles the stripped Ruby. The gem's `gem_init.c` loads that
   irep and then calls `mrb_aot_<gem>_register`, so the classes the
   Ruby opens exist when the cfuncs are added.

The generated C depends on the mrblib files, the gem's RBS, the
generator sources, the gem's `mrbgem.rake` (the flags live there) and
the Spinel binary. A change to any of them regenerates.

## 2. Build integration

A gem declares in its `mrbgem.rake` that it is to be compiled. The
author knows the gem's hot path and keeps the tests the twin runs:

```ruby
spec.aot = true                 # the typed methods
spec.aot_include_widened = true # the widened ones as well
spec.aot_exclude = %w[PSG::Driver#debug_dump]   # these stay in the irep
```

A target turns the machinery on. Without `conf.aot` a declaration
changes nothing. A build config may opt a gem out, and the block runs
after the gem's `mrbgem.rake`:

```ruby
conf.aot                                     # the Spinel submodule, built in place
conf.aot path: "/path/to/spinel"             # an explicit binary
conf.gem(core: 'picoruby-json') { |g| g.aot = false }   # opt out
```

Spinel is the submodule `lib/spinel` of this gem, pinned to the commit
the twin tests passed with and taken as is. A change goes upstream
first, then the submodule moves. The first build runs `make deps`
there, which fetches the vendored parsers (prism, rbs) from
rubygems.org once, then `make`. A later build rebuilds Spinel only when
the submodule's checkout moved. A build without `conf.aot` never
touches the submodule. `SPINEL=/path/to/spinel` in the environment
overrides the submodule; `path:` overrides both.

The R2P2 configs build with AOT when `PICORB_AOT=1` is set. FemtoRuby
(mruby/c) ignores `aot`. A gem without an mrblib keeps the plain path.

The gems that declare `spec.aot`: picoruby-json, picoruby-editor,
picoruby-midibase-mml, picoruby-keyboard, picoruby-uri and
picoruby-marshal (marshal with `aot_include_widened`). picoruby-aot-probe
declares it for the twin only.

## 3. Which defs compile

A `def` becomes a cfunc when all of these hold:

- Spinel typed every parameter and the return value (not "widened"),
  or `aot_include_widened` is set.
- The parameters are positional (required or optional), keyword
  (required or optional) and `&blk`. No `*rest`, no `**rest`, no
  anonymous `&`.
- The def is in a class or module body, is a `def self.x`, or is inside
  `class << self` (then it is a singleton method, private when a
  `private` precedes it there).
- The body uses only the supported syntax (section 3.1).
- The method does not wait for the scheduler (`sleep`, `sleep_ms`,
  `Task.pass`, `join`, `suspend`, a Task receiver, and on wasm the
  `_*_and_suspend` primitives behind `JS::Promise#await` and `JS.fetch`),
  directly or through another method of the gem or of a gem it depends
  on. A cfunc cannot yield to other tasks. Spinel types nothing inside
  a dependency, so there the search follows only what it can resolve: a
  bare call to the same scope, a call on a constant to that constant's
  singleton methods, and a call by the name of a def that reaches a
  primitive directly (`await`, `getch`). A call of this gem on an
  untyped receiver takes every def of its name, in the dependencies
  too: `kvs[key]` may be `IndexedDB::KVS#[]`, which awaits.
- The method is not a hook the VM calls while the irep loads:
  `inherited`, `included`, `extended`, `prepended`, `method_added`,
  `singleton_method_added`. The gem's own `include` runs the hook
  before the cfuncs are registered.
- No class-body code of the gem reads or calls the method's name when
  the irep loads: an `alias`, `alias_method`, a visibility call with a
  symbol (`private :x`, `module_function :x`), or a plain call at
  class-body level (a DSL such as `define_voice_program :kick, [...]`),
  wherever it sits (under an `if` too), and transitively every def such
  a method calls. Such code runs before the cfuncs are registered. A
  gem init that fails there is swallowed by `mrb_protect_error`, and
  every later gem stays uninitialized. The body of a `define_method`
  block, a `lambda`, a `proc` or a `Proc.new` at class-body level runs
  later and is not load-time code.

Both searches resolve a call as far as Spinel's type of its receiver
allows. A bare call reaches a method of the same class (singleton or
instance, as the caller is). A constant reaches the singleton methods
of the class it names (`Errors.new` is not `SQLite3::Database.new`), or
of any class when no def has that scope and the name is not a core
class; a value typed `class` reaches a singleton method of any class.
`X.new` reaches `X#initialize` as well. A String, an Array, an Integer
and the other core kinds reach only the defs of the gem that reopen
that class (`list.join` is `Array#join`, not `Task#join`). An object of
class X reaches the defs of X when the gem has any. A call on a
receiver Spinel did not type takes every def of its name, singleton or
not, in any scope.

Every other def stays in the irep. The reasons the sweep over every gem
shows, largest first: no type record (gems outside the host build, or
Spinel refused the gem), a wait for the scheduler, a name read at load,
a block literal at a call site, a rest parameter, `begin`/`rescue`,
`super`, `**rest`, a bare `raise`, `a[i] op= x`, a splat argument, a
binary string literal, `break` with a value, a dynamic constant path.
The counts move with every change; `tools/aot-gen --manifest` for one
gem, or the sweep in `aot-plan.md`, gives the current ones.

The structural reasons do not move with generator work: a def that may
wait stays in the irep, and so does everything that calls it. Def
counts are not speed. Whether a remaining def matters shows in a device
profile.

A polymorphic parameter is always widened. Spinel types a parameter from
the values the calls pass. A parameter that takes more than one kind of
value (`Marshal.dump(obj)`) is widened whatever the RBS says, and a call
of one kind narrows it to that kind. `aot_include_widened` is the way
to compile such defs.

### 3.1 Supported syntax

Literals (Array and Hash without a splat), locals, instance and global
variables, constants and constant paths, `if`/`unless`/`while`/`until`/
`case`, `return`, `break`, `next`, `and`/`or`/`not`, multiple assignment
to locals and instance variables, Range literals, string interpolation,
method calls (`&.`, keyword arguments and a forwarded `&blk` included;
a block literal at a call site is not), `yield`, `block_given?`, and
`raise` with a literal message or a constant class and a literal
message.

## 4. The generated C

Each def becomes two C functions: the impl, which takes `self` and the
parameters as C arguments, and the mruby cfunc, which reads the
arguments with `mrb_get_args` and calls the impl. The helpers the C
calls are in `include/picoruby_aot.h` and `src/picoruby_aot.c`.

### 4.1 Values

- A value is unboxed (`aot_int`, `mrb_float`, `mrb_bool`) when the C
  computes it itself (a literal, arithmetic on such values), or when
  Spinel proved its kind and that no nil reaches it (`"nullable":false`;
  an `Integer` and an `Integer?` share the tag `int`). A bool from the
  VM stays boxed. Every other argument, instance variable, constant and
  call result stays an `mrb_value`.
- An unboxed Integer is 64 bits on every target. A value crosses into
  the VM as an Integer when `mrb_int` holds it and as a Bignum otherwise
  (mruby-bigint, which every MRB_INT32 target links), and a Bignum that
  fits 64 bits is unboxed. Only past 64 bits does a compiled method
  differ from its irep: RangeError where the VM would grow the Bignum.
- A proved argument is read boxed and unboxed by a checked helper that
  takes the proved class and nothing else: a Float where an Integer was
  proved raises TypeError, as does an Integer where a Float was proved,
  or anything but `true` and `false` where a bool was proved. An RBS
  that misstates a class shows up as TypeError at the method's entry,
  never as a reinterpreted value. An argument stays boxed when the body
  tests its class (`is_a?`, `kind_of?`, `nil?`, `respond_to?`, `case`):
  such a guard is written for a caller the analysis never saw.
- A proved instance variable read or call result is unboxed with a
  checked helper at the first use that needs the C scalar, and never
  when it is only discarded, returned, tested or passed on. A value the
  analysis did not predict (a method redefined at run time) raises
  TypeError.
- The result of `yield` and of a call on a Proc stays boxed: Spinel
  typed it from the blocks at the call sites it saw, and another
  caller's block may return anything.

### 4.2 Operators and core methods

- Operators on boxed values go through guarded helpers (`aot_add`,
  ...). The helper computes the Integer and Float cases inline (and the
  String cases of `+`, `==`, `!=`, `<<`) and sends everything else to
  the VM, so nil, a Bignum and a user-defined operator behave as in the
  irep.
- An operator on two unboxed values is C arithmetic (`aot_int_add`,
  ...) with no guard. This assumes that the core operator is not
  redefined: Spinel types the expression under the same assumption, so
  a Ruby `Integer#+` would already have given it a wrong type. The VM
  honours such a redefinition (`bop_redefined`); a compiled method does
  not. Here the generated C trusts the analysis about a method instead
  of testing it.
- A String method on a receiver Spinel typed a String goes through a
  guarded helper (`size`, `length`, `bytesize`, `empty?`, `getbyte`,
  `[]`, `byteslice`): the String case reads the `RString`, any other
  receiver is sent to the VM by name. The guard answers nil as the irep
  does.
- An Array or Hash method on a receiver Spinel typed so goes through a
  guarded helper as well. `[]` and `[]=` take the test the VM's index
  opcodes take (`mrb->idx_class`); `size`, `length`, `empty?`, `push`,
  `pop`, `shift`, `first`, `last`, `clear`, `key?` and `delete` resolve
  the method as the VM would and compare it with the builtin recorded
  at gem init, so a redefinition on Array itself is honoured.
- A string with interpolation builds a new String and appends each part
  with `mrb_str_concat`, which converts with `to_s` as the VM does.
- `case/when` is an if chain. An Integer literal or an Integer Range
  pattern compares in place; a class asks `kind_of?`; the core scalars
  and a String compare as `==`; any other pattern is sent `===`.

### 4.3 Calls

- A call to another compiled method of the gem tests, with the method
  lookup the VM would do, that the name still resolves to that cfunc,
  and then calls its impl in C with no VM frame. A redefinition, an
  override in a subclass or another receiver class takes
  `mrb_funcall_argv`. A private method is called directly only from an
  implicit or `self` receiver. In a singleton method a call on `self`
  lands on another singleton method.
- Spinel's codegen record names the callee when it has one; else the
  generator takes a def of the same name in the receiver's class, or
  the one def of that name in the gem. The runtime test keeps the call
  correct whatever is chosen.
- An optional parameter reaches the impl as undef when the argument is
  not given, and the impl evaluates the default in method scope.
- Keyword parameters follow the positionals in the impl's argument
  list. The cfunc reads them with `mrb_get_args` and `:`: a required
  keyword takes its proved kind, an optional one is undef when not
  given. A missing or unknown keyword raises the VM's ArgumentError. A
  direct call maps the keywords by name onto the impl's slots. Any
  other call with keywords goes through `PicoRubyAOT.send_kw` (this
  gem's mrblib), which does `recv.__send__(mid, *args, **kw, &blk)` in
  the VM: mruby has no C API that passes keywords.
- A block the method receives (`&blk`, or a hidden parameter when the
  body has `yield` or `block_given?`) is the impl's last argument, a
  Proc or nil, read with `mrb_get_args "&"`. `yield` goes through
  `aot_yield`, which raises LocalJumpError "unexpected yield" with no
  block, as the VM does, else calls `mrb_yield_argv`. `&x` at a call
  site is forwarded to a compiled def's impl directly (`aot_block_arg`
  converts a Symbol with `to_proc`, as the send does) or to the VM with
  `mrb_funcall_with_block`. `break`, `next`, `return` and an exception
  in the block unwind through the compiled method as through any
  cfunc. What the block does is unknown to the generator: a block that
  waits for the scheduler under a compiled method cannot be switched
  away from, as a callee behind `mrb_funcall` cannot. The gem's author
  judges.
- `raise "msg"` and `raise Klass, "msg"` are raised in C; every other
  form goes to `Kernel#raise` through the VM.
- Other calls go through `mrb_funcall_argv`.

### 4.4 Constants, GC and the stack

- Constants resolve through the lexical scope of the def (its classes
  are recorded once, at registration), then the receiver's ancestors,
  then Object. A path `A::B::C` resolves `A` that way and reads `B` and
  `C` off the value before them, as the VM does; the last need not be a
  class.
- The VM's registers are GC roots; the generated C's locals are not. A
  value the C holds is safe while its owner references it or while it
  is in the GC arena (every allocation, every `mrb_funcall` result). A
  value whose owner may let go of it (an instance variable read, an
  element `[]`, `first`, `shift` or `pop` hands out, a Hash `[]` or
  `delete`) is pushed to the arena at the read, which protects it until
  the outermost cfunc returns to the VM. An immediate needs nothing.
- A direct call runs on the C stack, with no mruby frame: a backtrace
  does not list it, and deep recursion through compiled methods is
  bounded by the C stack. Each impl calls `mrb_c_stack_check` on entry.
  On a build with `MRB_USE_C_STACK_CHECK` (the R2P2 targets) a recursion
  that reaches the stack floor raises SystemStackError; without the
  macro the call is nothing.
- The generated C compiles without warnings under `-Wall -Wextra` on
  gcc and clang. A local the body only assigns (the result slot of an
  `if` used as a statement, a Ruby `_`) is not declared; each of its
  assignments keeps its right side for the effects, or goes when the
  right side has none. A comparison inside `if (...)` has no
  parentheses of its own.

## 5. Verification

The twin: the same picotest suite runs against three fresh builds of
the host test binary, the irep (the baseline), the AOT build
(`PICORB_AOT=1`) and the AOT build that also compiles the widened defs
(`PICORB_AOT_WIDENED=1`). The per-class and total summary lines must
match. A compiled method that behaves differently from its irep shows
up as a changed count.

```
rake "test:gems:aot_twin[picoruby-json,picoruby-midibase-mml]"
rake test:gems:aot_twin      # every gem test:gems:picoruby runs
rake test:gems:aot_twin32    # the same on the 32-bit host build
rake test:gems:aot_twin_wasm # the same on the wasm build
```

The host is 64-bit, so the twin there never meets a Bignum. The 32-bit
host build (`picoruby32-test.rb`: MRB_INT32, word boxing, mruby-bigint)
does. `picoruby-aot-probe` is a gem of typed methods the tests feed
values past the 31-bit fixnum and past `mrb_int`, plus the keyword and
block shapes; it exists for the twin and is not for a device build. The
wasm twin builds `picoruby-wasm-test.rb` with emcc three times and runs
the gems `test:gems:wasm` runs (funicular among them) under the Node
runner. CI runs the three twins (`test-picoruby-aot`,
`test-picoruby-aot32`, `test-picoruby-aot-wasm` in
`.github/workflows/c-cpp.yml`). The logs land under `build/aot-twin/`,
`build/aot-twin32/` and `build/aot-twin-wasm/`.

Before a gem is declared with `spec.aot`, it needs a picotest suite the
twin can run and a device measurement of the path that matters.

Measured on a pico2_w prod build, one irb line each, the median of
three runs, the irep against the same firmware with the gem compiled:

| Gem | Loop | Gain |
|---|---|---|
| json | `JSON.parse` x200 | 4.1x |
| midibase-mml | MML x10 | 1.65x |
| editor | `display_width` | 9.2x |
| uri | `URI.parse` x200 | 1.18x |
| marshal | `Marshal.load(Marshal.dump(h))` x200 | 2.14x (widened defs included) |

Flash: about +16 KB of ELF text for uri, vfs, vram and marshal
together, +51 KB for json, editor, mml and keyboard. RAM: about +1 KB.
The generated C is 2 to 3 times the size of the irep it replaces on
Thumb-2.

## 6. Tools and layout

- `tools/aot-gen --gem GEM --out-c FILE --out-rb DIR [--manifest FILE]
  [--exclude A#b,C.d] [--include-widened] [--types JSON]`: the generator
  the build calls. It runs Spinel itself unless `--types` is given
  (`SPINEL=/path/to/spinel`, or `spinel` on PATH).
- `tools/aot-analyze [--all | GEM...]`: run Spinel over gems and report
  how many defs are fully typed.
- `tools/picoruby_aot/`: the gem graph (`gems.rb`), the RBS-to-Ruby
  stubs (`stubs.rb`), the `RUBY_ENGINE` fold (`engine_fold.rb`), the
  Spinel driver and workspace (`spinel.rb`), the type map
  (`typemap.rb`), the classifier (`generate.rb`) and the code generator
  (`codegen.rb`).

```
include/picoruby_aot.h   helpers the generated C calls
src/picoruby_aot.c       their implementation (linked into libmruby)
mrblib/picoruby_aot.rb   PicoRubyAOT.send_kw
tools/                   host-side generator (Ruby, needs the prism and rbs gems)
lib/spinel               the Spinel submodule
docs/architecture.md     this document
docs/aot-plan.md         what comes next, and what was decided against
```
