# picoruby-aot: what compiles, what does not, and the work in between

This document is the working map of picoruby-aot. It says what the
generator compiles today, what it leaves in the irep and why, and what
work each gap needs. Update it with every change to the generator or the
runtime: the tables, the counts and the gem status.

Last update: 2026-10-15, branch `picoruby-aot` (Spinel 32e312d3). The
git log of this file dates each change.

## 1. What works

### 1.1 The pipeline

- Spinel runs on the host as the type oracle (`--emit-types`). The
  generator reads its JSON and the Prism AST and writes one C file per
  gem against the mruby API. Spinel is a submodule of this gem. Spinel
  reads a copy of each mrblib file with its `RUBY_ENGINE` branches
  folded for mruby (section 2), since it would otherwise analyze the
  mruby/c side.
- A gem opts in with `spec.aot = true` (and `spec.aot_include_widened =
  true` for the defs Spinel widened to untyped). A target turns the
  machinery on with `conf.aot`. Today json, editor, midibase-mml and
  keyboard declare it; the R2P2 configs build with it when
  `PICORB_AOT=1` is set.
- Each compiled def is two C functions: the impl (C arguments) and the
  mruby cfunc. A call between compiled defs of one gem goes impl to impl
  when the method still resolves to the cfunc.

### 1.2 Semantics the generated C keeps

- A proved Integer is `aot_int`, 64 bits on every target. A value
  crosses into the VM as an Integer or, past `mrb_int`, as a Bignum
  (mruby-bigint); a Bignum that fits 64 bits is unboxed. Only past 64
  bits does a compiled method differ from its irep (RangeError).
- A proved parameter takes its proved class and nothing else: a wrong
  class raises TypeError at entry, never a reinterpreted value.
- Operators and the core String, Array and Hash methods go through
  guarded helpers: the builtin case in C, everything else to the VM, so
  nil, a redefinition and a user class behave as in the irep.
- Constants resolve as the VM does: the lexical scope, the ancestors,
  Object; a path `A::B::C` name by name.
- Every impl calls `mrb_c_stack_check` on entry (MRB_USE_C_STACK_CHECK
  on the R2P2 targets): deep recursion raises SystemStackError.
- A block the method receives (`&blk`, `yield`, `block_given?`) is the
  impl's last argument, a Proc or nil. `yield` with no block raises
  LocalJumpError as the VM does. The value a block returns stays boxed:
  Spinel typed it from the blocks at the call sites it saw, and another
  caller's block may return anything. `break`, `next`, `return` and an
  exception in the block unwind through the compiled method as through
  any cfunc (the probe tests each).
- Values the C holds are GC roots through the arena (see the README).

### 1.3 Verification

- The twin: the same picotest suite on the irep build, the AOT build and
  the AOT build with widened defs; the summaries must match.
  `rake test:gems:aot_twin` (every gem) is SAME on the host (103 summary
  lines, 42 gems). CI runs it.
- `rake test:gems:aot_twin32`: the twin on the 32-bit host build
  (MRB_INT32, word boxing, mruby-bigint), where a Bignum shows. SAME over
  every gem (87 lines, 36 gems; the socket gems are out on 32-bit). CI
  runs it. `picoruby-aot-probe` is the gem of typed methods the tests
  feed values past the fixnum and past `mrb_int`.
- Device (pico2_w, prod): JSON x200 12.8 s to 3.1 s (4.1x), MML x10
  2.36 s to 1.44 s (1.65x), editor `display_width` 5.9 ms to 0.64 ms
  (9.2x). ELF text +41 KB for the four gems before the 64-bit Integer,
  +10 KB more with it. RAM +1.2 KB.
- Device, the owner's gems (pico2_w, prod, 2026-10-14; one irb line
  each, `Machine.uptime_us`, the median of three runs; A is the irep,
  B the same firmware with uri, vfs, vram and marshal compiled):

  | Loop | A | B | B / A |
  |---|---|---|---|
  | `URI.parse` x200 | 482 ms | 408 ms | 1.18x (1.26x on another flash of the same build) |
  | `File.expand_path` + `File.dirname` x200 | 1190 ms | 1354 ms | 0.88x |
  | `VRAM::Delegatable#set_pixel` x10000 | 800 ms | 778 ms | 1.03x |
  | `VRAM::Delegatable#draw_rect` x2000 | 901 ms | 865 ms | 1.04x |
  | `Marshal.load(Marshal.dump(h))` x200 | 5709 ms | 2666 ms | 2.14x (18 of 19 defs, with widened; 1.76x on the flash before the stub fix) |
  | free heap after boot | 128,912 | 128,376 | -0.5 KB |
  | ELF text | 2,432,096 | 2,448,136 | +16.0 KB for the four gems |

  uri declares `spec.aot` since, marshal `spec.aot` and
  `spec.aot_include_widened` (its widened defs are the point: section
  2, "a polymorphic parameter"). vfs lost: `File.expand_path`
  compiles, but its work is one call into `VFS.sanitize`, which stays in
  the irep (named at load through `VFS.pwd`), and a C-to-VM call costs
  more than a VM-to-VM one. vram's `Delegatable` is a bounds check and a
  call into C `VRAM`, which the VM dispatches as cheaply. vim's hot loop
  (`_start`) has a block. The three stay as they are (section 4).
- Device, yaml (same method, firmware C = uri, marshal and yaml
  compiled, yaml with widened defs, 11 of 13): `YAML.load` of the README
  document x200 6227 ms to 5241 ms (1.19x), `YAML.dump` of it x200
  1820 ms to 1603 ms (1.14x); the compiled object is 6.6 KB of text.
  The parser's work per line is `split`, `strip`, `start_with?` and
  Hash and Array operations, all C builtins the generated code calls
  as the VM does; only `is_integer?` is a Ruby loop. Not declared.

### 1.4 Syntax the generator accepts

Literals (Array and Hash without a splat), locals, instance and global
variables, constants and constant paths, `if`/`unless`/`while`/`until`/
`case`, `return`, `break`, `next`, `and`/`or`/`not`, multiple assignment
to locals and instance variables, Range literals, method calls (`&.`,
keyword arguments and a forwarded `&blk` included; a block literal at a
call site is not), `yield`, `block_given?`, `raise` with a constant
class. Parameters: positional and keyword, required or optional, and
`&blk` (no `*rest`, `**rest`). Defs in a class or module
body, `def self.x`, and defs inside `class << self` (singleton methods;
`private` there makes them private methods of the singleton class).
A def whose name class-body code of the gem reads or calls stays in the
irep: an `alias`, `alias_method`, a visibility call with a symbol
(`private :x`, `module_function :x`), or a plain call at class-body
level (a DSL such as psg's `define_voice_program :kick, [...]`), wherever
it sits (under an `if` too), and transitively every def such a method
calls (`define_voice_program` calls `voice_program`). Such code runs when
the irep loads, before the cfuncs are registered. drb's `create_socket`, irq's
`private :_stop_locked, ...` and psg's `define_voice_program` showed it:
the gem init failed inside `mrb_protect_error`, silently, and every
later gem stayed uninitialized.

## 2. What stays in the irep, and why

The sweep over every gem (1083 defs on the host build, widened
included) leaves 361 in the irep; 722 compile (544 typed). The reasons,
largest first (the counts are from the sweep before `class << self`
bodies were walked, except the last row):

| Reason | Defs | Kind |
|---|---|---|
| Waits for the scheduler (`sleep_ms`, `submit`, `join`, ... reached through the call graph) | about 150 | Structural |
| `initialize` with `super`, a block, a rest parameter or a wait | 14 | The other 56 compile since 2026-10-14 (3.8) |
| Keyword parameters (30) and keyword arguments (14) | 44 | Done 2026-10-14 (3.2); `**rest` stays out (2 defs) |
| Blocks: a block literal at a call site (24); a block parameter (16) and `yield` (3) compiled since 2026-10-15 | 24 | Generator gap, design (3.5). The 2026-10-15 sweep over every gem (2651 defs, the wasm and shell gems included) has 57 block literals left and no other block reason |
| Rest and splat parameters and arguments | 26 | Generator gap, and a Spinel issue |
| `begin`/`rescue`/`ensure` | 19 | Generator gap |
| Other (`super`, `a[i] op= x`, `a[i] ||= x`, `break` with a value, bare `raise`, a literal past 64 bits) | about 10 | Scattered |
| A name class-body code reads or calls when the irep loads, and what those methods call: `alias`, `alias_method`, `private :x`, a DSL call (drb, irq, psg, sqlite3, regexp) | 19 | By design; `private :x` could instead be honoured by the generator and blanked from the Ruby |

One gap the sweep does not count, because the generator never sees the
def as a candidate:

| Gap | Where it shows |
|---|---|
| Spinel widens a def to untyped although the RBS gives every parameter a class (closed 2026-10-14) | `VRAM::Delegatable#set_pixel`, `#draw_line`, `#draw_rect` |
| Spinel folds `RUBY_ENGINE` as "spinel" (closed 2026-10-14) | net-http's `get`, every `if RUBY_ENGINE == 'mruby'` |
| The stub generator took a def inside `class << self` for undefined and stubbed it; the seed landed on the stub (closed 2026-10-14) | every `VFS.*` and `File.*` of vfs, all of marshal: `(untyped) -> String` beside a `(String) -> String` signature |

Root cause (2026-10-14): Spinel's `spinel_rbs_extract` dropped a whole
signature over one parameter it could not type (an optional positional
such as `?Integer color`, a rest parameter, a `self` return), so a
method no call site binds had no seed left and widened. Fixed upstream
in matz/spinel PR #9086 (each parameter keeps its own field; keyword
fields carry their names), merged the same day; the submodule is at
32e312d3 since, and the twin types vram 5 of 5 and stays SAME. The gap
is closed.

The bump surfaced a second oracle gap: Spinel answers `RUBY_ENGINE`
with "spinel" and drops the branch a `RUBY_ENGINE == "..."` check rules
out before it analyzes, so it kept the mruby/c side of every `if
RUBY_ENGINE == 'mruby' ... else ... end` in the mrblibs (12 gems have
one). net-http's mruby/c `get` passes its arguments shifted, and once
the seeds applied, that dead code was a contradicted seed that aborted
the gem (0 of 44 defs). The workspace now hands Spinel a copy of each
mrblib file folded for mruby (`EngineFold`: the comparison reads `true`
or `false`, the dead branch is blank, `case RUBY_ENGINE` keeps its
`when "mruby"`; columns and lines do not move, and the records map back
to the source). net-http types 15 of 44.

(Defs inside `class << self` were such a gap until 2026-10-14: the
walker skipped the whole body. marshal went from 0 to 18 of 19 defs and
vfs from 26 to 53 of 71 when it was closed.)

A polymorphic parameter is always widened. Spinel types a parameter
from the values the calls pass; a parameter that takes more than one
kind of value (`Marshal.dump(obj)`) is widened whether a call binds it
or not, and whatever the RBS says, since the seed cannot narrow a
parameter whose body is reached with several kinds. Tried 2026-10-14 on
marshal: an RBS signature for `dump` and `load` (as a `| ...` overload,
which steep requires beside the core declaration, with the `| ...`
folded away for Spinel), a seed file of calls fed to Spinel only, and
calls of one kind, of several scalar kinds, of Array and Hash, and of
an untyped value; every form leaves `dump`, `dump_object` and the
`load_*` family widened, and a call of one kind narrows the parameter
to it. The way to compile such defs is `spec.aot_include_widened`,
which marshal declares; its 18 of 19 are widened defs, and the sweep's
"544 typed" counts them as typed because the sweep honors the gem's
flag (532 without them). A flag change regenerates since 2026-10-14
(mrbgem.rake is a prerequisite of the generated C).

The structural reasons do not move with generator work. A cfunc cannot
yield to other tasks, so a def that may wait stays in the irep, and so
does everything that calls it. The reachable ceiling for the generator
is therefore about 930 of 1083 defs.

Def counts are not speed. The hot paths of the declared gems are already
compiled; whether a remaining def matters shows in a device profile, not
in this table.

## 3. The work each gap needs

Ordered by the ratio of defs gained to design needed.

### 3.1 `class << self` bodies (done 2026-10-14)

The walker takes `Prism::SingletonClassNode` whose expression is `self`
as the enclosing scope with every def a singleton method; `private`
inside it applies, and such a def registers as a private method of the
singleton class. A `def self.x` after a bare `private` in a class body
is public, as in Ruby (it was registered private before).

### 3.2 Keyword parameters and arguments (done 2026-10-14)

- Parameters: `mrb_get_args` with `:` (`mrb_kwargs`); a required keyword
  takes its proved kind (Spinel types `RequiredKeywordParameterNode`),
  an optional one is undef when not given and evaluates its default in
  method scope, as an optional positional does. `**rest` stays out.
- Arguments: a direct call to a compiled def of the gem maps the
  keywords by name onto the impl's slots in C. Every other call, and the
  fallback when the method was redefined, goes through
  `PicoRubyAOT.send_kw` (mrblib of picoruby-aot), which does
  `recv.__send__(mid, *args, **kw)`: mruby has no C API that passes
  keywords, and OP_ENTER takes a trailing Hash from `mrb_funcall_argv`
  as a positional. A `mrb_funcall_argv_kw` in mruby would make that
  path one frame cheaper (3.7).
- Found on the way: a self call inside a singleton method looked for
  `Scope#name` and never found `Scope.name`, so singleton methods never
  called each other directly. They do now (marshal: 31 direct calls).

### 3.3 Rest and splat (Spinel first)

Spinel types a rest parameter `*Integer` as `poly_array`, so the def
widens. File a Spinel issue, or accept widened-only compilation: the
generator can read `*rest` as an Array (`mrb_get_args "*"`) in the
widened mode without any typing. 26 defs, the regexp wrappers among
them.

### 3.4 `begin`/`rescue`/`ensure` (design)

A rescue in C needs `mrb_protect_error` or `mrb_rescue_exceptions`
around the body as a nested C function, with the locals the body
assigns passed by reference; `ensure` needs `mrb_ensure`. The body
inside the closure cannot `return` from the method directly. Decide
first whether to compile only the forms without `return`/`break`
inside the protected body. 19 defs, mostly net and midibase.

### 3.5 Blocks (the receiving side done 2026-10-15; a block literal: design)

A def is on the receiving side when its caller gives it the block: a
`&blk` parameter, `yield`, `block_given?`. The block is a Proc the VM
made; the generated C never builds one. Done:

- `&blk` is the impl's last C argument, read with `mrb_get_args "&"`;
  a def with `yield` or `block_given?` and no `&blk` has it as a hidden
  argument. A direct caller passes the block it forwards, or nil.
- `yield` goes through `aot_yield`: LocalJumpError "unexpected yield"
  when there is no block (what OP_BLKPUSH raises), else
  `mrb_yield_argv`. `blk.call(x)` is an ordinary call.
- `&x` at a call site is forwarded: to a compiled def's impl directly
  (`aot_block_arg` converts a Symbol with `to_proc`, as the send does),
  to the VM with `mrb_funcall_with_block`, with keywords through
  `send_kw`.
- The result of `yield` and of a call on a proc-typed receiver stays
  boxed (section 1.2).
- The probe gem tests each shape: a block called in a loop, an outer
  local the block writes, `break` with a value, `next`, `return` from
  the block, an exception, `yield` twice, a Bignum and a String from
  the block, `yield` without a block, `block_given?`, `&blk` to a
  compiled def, `&:to_s` to a compiled def and to `map`, `&blk` with
  keywords.

What the block does is unknown to the generator. A block that waits
for the scheduler under a compiled def cannot be switched away from, as
a callee behind `mrb_funcall` cannot (section 2); the gem's author
judges, as for any call into the VM.

A block literal at a call site (24 defs, `File.open(path) { |f| ... }`
in most of them) is the passing side, and the generated C would have to
make the Proc. How Spinel does it: a block is never a closure by
default. A builtin iterator becomes a C `for` loop with the block body
inside and the block parameters as method locals; a Ruby method that
yields is inlined into its caller; `next` is `continue`, `break v` a
`goto` out of the loop, `return` the method's C return. Only a block
that is stored or passed on becomes a C function with a capture struct,
and a captured local the block writes becomes a GC cell both sides read
through. Spinel owns the whole program; picoruby-aot shares the VM, and
mruby has no `break` or method `return` from a cfunc Proc (nothing in
the C API raises the VM's break). Options, in the order to try:

1. A cfunc Proc with an env (`mrb_proc_new_cfunc_with_env`): `self` and
   the outer locals the block reads travel in the env, the call site
   hands it over with `mrb_funcall_with_block`. A block with `break`, a
   `return`, a write to an outer local or a nested block keeps the def
   in the irep. The `open { |f| }` shape is exactly this; the aim is
   that the def compiles, not that the block is fast. An outer local the
   block writes can follow later as a one-element Array in the env.
2. A block over a core receiver with `each`, `times`, `each_byte`,
   `downto`, where Spinel's record says `inlined: true`, becomes a C
   loop with the guarded element access when `aot_method_is` says the
   iterator is still the builtin; the Proc of option 1 is the fallback
   when it is not. `next`, `break` and `return` map as Spinel maps them.
   The owner's gems have almost no hot block loop (the `while` rule),
   so this waits for a device profile that shows one.
3. Leave the def in the irep and rewrite the hot loop with `while`.

### 3.6 Scattered

- `a[i] op= x` and `a[i] ||= x`: read, operate, write with the guarded
  index helpers.
- `break` with a value: the loop's result temp.
- Bare `raise` in a rescue: needs 3.4.
- `super`: `mrb_funcall_super` does not exist in C; needs the method
  search from the owner class upward. Rare; leave.

### 3.7 Beyond the generator

- A per-class "builtins untouched" bit in mruby, so the guarded helpers
  test one bit instead of a method search. Measure first whether
  MRB_METHOD_CACHE is on in the PicoRuby builds and what the search
  costs in a profile.
- A lighter funcall for a resolved method (no re-lookup). Same measure.
- `mrb_funcall_argv_kw(mrb, self, mid, argc, argv, kw_hash)`: a funcall
  that sets the frame's keyword flag, so a call with keywords from C
  needs no Ruby trampoline (3.2). An mruby proposal.
- `SystemStackError#message` prints the class name, not "stack level
  too deep" (mruby follow-up).
- A gem init that raises (an irep that fails to load, a registration
  that cannot find its class) is swallowed by `mrb_protect_error` in
  `init_mrbgems_body`, and every later gem stays uninitialized; the
  first symptom is `require` answering false. The twin shows it as
  "Crash in loading" on every test. A loud failure at init would be a
  PicoRuby or mruby change worth making.
- Spinel: rest parameter typing (3.3); the widening in 2 above.

### 3.8 `initialize` (done 2026-10-14)

`initialize` stayed in the irep as an MVP policy ("runs once per
object, its irep is small"), not for a technical reason: a C
`initialize` is what every C gem has, and `Class#new` reaches it the
same way. Measured on the host before lifting the policy (all gems,
1083 defs):

| | Policy on | Policy off |
|---|---|---|
| typed defs compiled | 466 | 494 |
| widened included | 666 | 722 |
| gem_init.o text + data (the ireps) | 419,660 | 412,316 |
| AOT objects text (host, unoptimized) | 827,766 | 873,132 |
| twin (irep / aot / widened) | SAME | SAME |

Of the 70 `initialize` defs 56 compile; 14 stay for `super` (net-http's
exception classes, editor), a block (vim), a rest parameter or a wait.
The ireps are static structs in flash, so the gain is flash, not RAM:
7 KB of irep for 45 KB of unoptimized host C, which `-Os` on the device
brings down to the usual ratio (section 1.3). The speed gain is limited
to classes created in loops (uri's `URI::Generic`, json's parser
objects, vfs's `File` and `Dir`). The generator registers a compiled
`initialize` with `mrb_define_private_method`, as `def initialize` in
the irep is private.

## 4. The gems the owner wants compiled

Counts from `tools/aot-gen` at the commit above, with
`--include-widened`. "Blocked by" names what keeps the rest out.

| Gem | Defs the generator sees | Compiled (typed / widened) | Blocked by | Expected gain |
|---|---|---|---|---|
| picoruby-marshal | 19 | 6 / 18 | `encode_fixnum` has a binary string literal. `dump`, `dump_object` and the `load_*` family are widened: a polymorphic parameter (section 2). Declares `spec.aot` and `spec.aot_include_widened` (device: 2.14x with the 18) | High: dump/load walk bytes in Ruby, and the 31 calls between its singleton methods are direct |
| picoruby-regexp | 11 | 2 / 4 | rest parameters on the String wrappers (3.3), a block literal in `__sub_common` (3.5; `__sub_each` and `scan` compile since 2026-10-15) | Low: the engine is C; the wrappers are thin |
| picoruby-uri | 7 | 5 / 7 | nothing (`initialize` compiles since 3.8). Declares `spec.aot` (device: 1.2x) | Moderate: `parse`, `request_uri`, `encode_www_form` are string loops |
| picoruby-vfs | 71 | 22 / 40 | rest parameters on `gets`, `printf`, `chmod`, `utime` (3.3); block literals in `Dir.glob` and `load_file` (3.5; `each`, `each_line` and `Dir.chdir` compile since 2026-10-15); `File.open`, `Dir.open`, `VFS.sanitize` and 20 more named at load (`alias`, `private :x`, `VFS.pwd`); `VFS.split`, `sanitize_and_split`, `File.dirname` widened (a tuple return). Not declared: the device ran 0.88x (section 1.3) | Moderate: the file I/O is C, the path and mode logic is Ruby. picotest since 2026-10-14 (66 assertions on a RAM littlefs) |
| picoruby-vim | 6 | 3 / 4 | block literals in `_start` and `initialize` (3.5). Not declared: the key loop is `_start`, and the four that compile run once per command | Low here, high through editor: Vim runs on `Editor::Screen`, which is compiled already. picotest since 2026-10-14 (17 assertions: the command line and the file round trip; the key loop needs a terminal) |
| picoruby-vram | 5 | 5 / 5 | nothing (keywords done; the Spinel widening of section 2 closed with the submodule at 32e312d3). Not declared: the device ran 1.03x, since `Delegatable` only forwards into C | Low: `VRAM` itself is C; `Delegatable` forwards |
| picoruby-yaml | 13 | 5 / 11 | `load`, `dump`, `parse`, `serialize`, `parse_value` and the two split helpers are widened (a YAML value is any type); a block in `load_file`; a bare `raise` in `is_integer?`. Not declared: the device ran 1.19x on load and 1.14x on dump (section 1.3) | Low: the parser is line-based on C String methods. picotest since 2026-10-14 (31 assertions) |

Before any of these is declared with `spec.aot = true`, the gem needs a
picotest suite the twin can run (all seven have one now) and a device
measurement of the path that matters.

Writing the vfs and vim suites (2026-10-14) found, besides the tests:

- Three vfs bugs, fixed with the suite: `File#putc` switched on
  `ch.class` and always raised, `File#rewind` called the driver's seek
  with one argument, `File.unlink` always answered 0.
- `Dir#path` called `fullpath` on the littlefs Dir, which no backend
  defines. Fixed on master (032d2fd3: the Dir keeps the path it was
  opened with); the suite reads it since ded2355f.
- The host test binary carries mruby-dir and mruby-io (the mruby-posix
  gembox). picoruby-vfs reopens File and Dir in Ruby, and the gem that
  initializes last owns a method both define; mruby-dir came after vfs,
  so `Dir.mkdir` ran on the host filesystem (EROFS at "/"). Under
  `TEST_TASK` vfs now depends on mruby-dir and mruby-io, which puts it
  last.

## 5. How to keep this document current

- After a generator change: run `rake test:gems:aot_twin` and
  `rake test:gems:aot_twin32`, then refresh the counts in section 2 and
  the gem table in section 4 (`SPINEL=mrbgems/picoruby-aot/lib/spinel/bin/spinel
  ruby mrbgems/picoruby-aot/tools/aot-gen --include-widened --gem GEM ...
  --manifest FILE`).
- After a semantics change: add it to section 1.2 and to the README.
- After a device measurement: add it to section 1.3 and, for a gem in
  section 4, to its row.
- When a gem is declared with `spec.aot`: move it from section 4 to
  section 1.1 and note the twin result.
