# picoruby-aot plan

This document lists the work to do, the work that may be done, and the
decisions against doing something. What picoruby-aot does today is in
`architecture.md`. When an item here is done, it moves there and leaves
this document. The aim is a small document of decisions, which then
merges into `architecture.md`.

## 1. Work to do

Ordered by the ratio of defs gained to design needed.

### 1.1 A block literal at a call site

A block literal is a cfunc Proc with an env (architecture 4.3). An
outer local the block writes, or reads while the method rewrites it,
is a cell both sides share. The block cannot do a `break` or a method
`return`: nothing in the C API raises the VM's break. A def still
stays in the irep when its block holds another block or takes a
parameter other than a required one. On the host build 27 defs carry
a block cfunc (keyboard 6, socket 3, markdown 2, drb, machine,
picoline, regexp, uart, vfs one each, the probe 10); 2 defs stay in
the irep for the block alone (one `break`, one `return`).
`File.open { |f| }` compiles in vfs: the wait search types
the local `file = self.new(path)` and the C class of `@file`
(architecture, "Both searches resolve a call"). The callers in editor,
vim and yaml still wait, because there `File.open` is a def of a
dependency, which Spinel does not type: `@file.close` resolves by name
to `Net::WebSocket::Client#close` (1.4). vfs `load_file` waits through
`f.read` on the block parameter (1.4).

How Spinel does it: a block is never a closure by default. A builtin
iterator becomes a C `for` loop with the block body inside and the
block parameters as method locals; a Ruby method that yields is inlined
into its caller; `next` is `continue`, `break v` a `goto` out of the
loop, `return` the method's C return. Only a block that is stored or
passed on becomes a C function with a capture struct, and a captured
local the block writes becomes a GC cell both sides read through.
Spinel owns the whole program; picoruby-aot shares the VM.

Options, in the order to try:

1. A nested block that reads outer locals: the same env chain one
   level further (a cell of the outer method passes through the
   middle block's env).
2. A block over a core receiver with `each`, `times`, `each_byte`,
   `downto`, where Spinel's record says `inlined: true`, becomes a C
   loop with the guarded element access when `aot_method_is` says the
   iterator is still the builtin; the env Proc is the fallback when it
   is not. `next`, `break` and `return` map as Spinel maps them. The
   owner's gems have almost no hot block loop (the `while` rule), so
   this waits for a device profile that shows one.
3. Leave the def in the irep and rewrite the hot loop with `while`.

### 1.2 `begin`/`rescue`/`ensure`

A begin compiles (architecture 4.3). On the host build 14 gem defs
carry one (socket 3, websocket 4, midibase 3, adc, benchmark, mqtt,
sqlite3 one each; the probe 9 more). The other 57 defs with a begin
stay in the irep for a wait or a widened parameter, not for the begin.
What is left of the syntax: a begin inside a block literal (picoline
`readline` and `ask`), a bare `raise` (yaml `is_integer?`), `retry`,
`else`, and the `x rescue y` modifier (one site in the aot gems).

### 1.3 Rest and splat

Spinel types a rest parameter `*Integer` as `poly_array`, so the def
widens. File a Spinel issue, or accept widened-only compilation: the
generator can read `*rest` as an Array (`mrb_get_args "*"`) in the
widened mode without any typing. About 45 defs, the regexp String
wrappers among them.

### 1.4 Scattered

- `a[i] op= x` and `a[i] ||= x`: read, operate, write with the guarded
  index helpers.
- `break` with a value: the loop's result temp.
- `super`: `mrb_funcall_super` does not exist in C; needs the method
  search from the owner class upward. Rare.
- `private :x` and the other visibility calls with a symbol could be
  honoured by the generator and blanked from the Ruby, instead of
  keeping the named def in the irep.
- Adjacent string literals with interpolation (`"a #{x}" \ "b #{y}"`):
  Prism nests an InterpolatedStringNode in an InterpolatedStringNode
  and the codegen refuses the inner one. About 40 defs in funicular,
  all error messages.
- Receivers inside a dependency: Spinel types nothing there, so the
  wait search resolves a call on a local or an instance variable of a
  dependency def by name alone (`@file.close` in vfs `File#close` is
  `Net::WebSocket::Client#close` when editor, vim or yaml is the gem).
  Left as is (2026-10-19): it holds back 5 defs (editor 2, vim 2, yaml
  1), all the `File.open { }` shape, on paths that run once. If it is
  ever worth it: `aot-gen` runs Spinel for each declared dependency
  into the workspace (cached by the mtimes of its mrblib and sig),
  merges those records into the TypeMap, and the foreign branch of the
  wait search resolves receivers through `resolve_receiver`. The
  `:source` mode of the workspace is not the way (slower, and a refusal
  in a dependency sinks the gem).
- A block parameter of a call whose def yields a typed value
  (`File.open(path) { |f| f.read }`: `yield(file)` with the typed
  local) is poly; vfs `load_file` waits through it.
- A constant that names a C class (`SQLite3::Backup.new`) reaches
  every `initialize` of the build; the C-class rule of `obj_X` could
  apply, but `direct_wait?` takes a constant receiver that resolves
  to no scope as a scheduler call by name (a `sleep` or `join` on a C
  class), so the change needs a wait list of the C classes first.

### 1.5 picoruby-shell and picoruby-funicular: the defs still in the irep

Funicular compiles 173 typed / 396 widened of 756 defs on the wasm
build (vdom.rb 29 of 35, differ.rb 9 of 9, patcher.rb 5 of 5; the VDOM
path from `Differ.diff` through `Patcher#apply` and `Renderer#render`
is compiled). The measurement is in `architecture.md`, section 5.
Spinel refuses 12 sites: four by the String-sharing seal (a mutable
String handed on by a route the rule cannot carry yet, matz/spinel#9258;
`component.rb:1054`, `db.rb:2410`, `html_serializer.rb:75`,
`patcher.rb:250`) and eight by codegen (`instance_method`,
`instance_exec`, `private_instance_methods`, `define_method` with a
computed name, `Object#extend`, a VNode argument no conversion keeps).
`--emit-types` writes the JSON with the refusals in it, so the defs of
a refused unit stay in the irep and every other unit is typed. What
keeps the rest in the irep, and what would free it:

- The shell's exec path (21 defs through `Sandbox#wait`, the setup
  sleeps, `getch`) is not planned: a launch/await split with the wait
  at `run_shell` would free it, and the parser is where the time was.
- funicular: the types. On wasm a JS call may suspend the task
  (`JS::Promise#await`, `IndexedDB::Store#[]`), and the wait search
  takes a call on an untyped receiver for every def of its name, so an
  untyped `x[]`, `x.each` or `x.empty?` keeps its caller in the irep
  (about 60 defs reach `IndexedDB::KVS` that way). What is behind the
  waits, by their roots (the first call of each excluded def that is a
  primitive or resolves to another gem's def):
  - A Hash value or an Array element Spinel can only box
    (`@messages.each { |attr, msgs| msgs.each }`: `msgs` is poly) on
    about 50 roots, which then meet `IndexedDB::Store#each`, `#empty?`,
    `#keys`, `#delete`. Spinel has no typed-value Hash beyond `int` and
    `string` values, so the RBS (`Hash[Symbol, Array[String]]`) cannot
    say more. A Spinel topic, not a gem one.
  - The `Hash[Symbol, ...]` seeds: the RBS declares 164 such slots,
    and of the 82 the JSON types from the gem's own Ruby, 49 give way
    to untyped. Their sources (the `why` chain of each) are a value
    born untyped at the call (an untyped ivar `@suspense_data`, a Proc
    call `params_proc.call(self)`, an untyped return
    `styles_definitions`), an empty literal default `props = {}`
    (`ViewContext#tag`, `#component`, `#build_element`,
    `#normalize_props`), an element of a poly Array (`patch[1]`,
    `hash[key] || {}`), and returns the compiler records no source for.
    The order is: type the sources (a typed constant for an empty
    default, the ivars and returns), and only then look at the seed
    rule. The refusals `0_tags.rb:41` (the `define_method` loop; the
    generator's stub carries the RBS signatures of the tags instead)
    and `router.rb:19` (`Object#extend` of a `Module.new`, which no
    AOT form expresses) stay.
  - What vdom.rb keeps in the irep is a `return` in a block (the two
    validators, 1.1), `super` in three `initialize` (1.4) and a rest
    parameter (1.3).
  - For the defs the declarations cannot settle: a run-time class check
    at a def's entry that hands a receiver of a waiting class to the
    irep copy.
  - What remains in the browser profile of the diff, after the
    generator rounds and WebAssembly exceptions: the GC (about a sixth
    of the diff; Patch objects, the key map, temporaries, a funicular
    topic), the method lookups before each direct call and reader
    (`aot_method_is`, `aot_method_same`, `aot_read0`, about a tenth;
    1.6), and the DOM calls. The profile recipe: `--profiling-funcs`
    keeps the names (`PICORB_WASM_PROFILING=1`), a Chrome Performance
    trace, and the bench pages in `~/work/funicular-bench`.
  - A seeded String container that receives shared Strings settles in
    its shared form (a poly Array whose boxes hold the handles,
    matz/spinel#9626), and `--emit-types` reports it as `Array[untyped]`
    with `widened`: `index_name`, `quoted_list`, `get_state_keys` are
    widened for that (they still compile under `--include-widened`). A
    report fix is drafted on the Spinel side (branch
    `fix/emit-types-shared-seed`, WIP) and paused while the Array seed
    rule on master settles.
  - The same collision reaches marshal on the wasm build: `dump_hash`
    calls `keys` on its untyped object, which could be
    `IndexedDB::Store#keys` (an await), so marshal's dump path is in
    the irep there (14/19 typed on wasm, 18/19 on the host and the
    device, which carry no IndexedDB).
  - A class-valued receiver (`component_vnode.component_class.new`,
    RBS `singleton(Component)`, Spinel `class`) reaches every
    `initialize`; the record's `rbs` string could narrow it.
  - A `Hash[Integer, ...]` seed is dropped by the extractor (no kind).

### 1.6 Beyond the generator

Two ideas cut the method searches that the generated C makes. The
device build uses `MRB_CONSTRAINED_BASELINE_PROFILE`, which defines
`MRB_NO_METHOD_CACHE`, so on the device every search walks the ancestor
chain and its khash tables.

The profile (pico2_w prod with AOT, an irb loop halted at random by gdb
60 times, the innermost frame counted; the recipe is `gdb_sample.sh`
and `tally_samples.rb` in the session scratchpad: halt, `bt 10`,
continue, with `set pagination off`):

| Loop | Samples in `mrb_vm_find_method` | From `aot_method_same` / `aot_method_is` |
|---|---|---|
| `Shell::Parser` on a 33-character pipeline | 21 of 60 | 4 |
| `JSON.parse` of a 90-byte object | 8 of 60 | 7 |

In the shell 17 of the 21 came from `Array#include?` on the tokenizer's
character sets: `mrb_equal` searched `String#==` for every element, and
`mrb_funcall_argv` searched it again. The tokenizer tests its character
sets with `String#include?` now (one C scan), and mruby has an
`idx_class` slot for `String#==` so that `mrb_equal` and `OP_EQ` compare
two Strings from C (mruby f53fa6d96, in lib/mruby). In JSON 7 of the 8
were `aot_nil_p`: `parse_string` tested every plain byte of a string
with `byte.nil?`, a search for `Kernel#nil?` from Integer. The test is
a truth test now (`elsif byte`): `JSON.parse` of a 91-byte object takes
3.7 ms compiled instead of 5.0 ms on the pico2_w. What remains for the
guards of the generated C (`aot_method_is`, `aot_method_same`) is 4 of
60 samples in the shell and 1 of 60 in JSON: the epoch counter below
would gain that much on these paths, so it waits for a path where the
share is larger.

- A lighter funcall for a resolved method. `aot_method_is` and the
  guarded helpers search the method, then fall back to `mrb_funcall_argv`,
  which searches it again inside `mrb_funcall_with_block`. An mruby API
  that takes the `mrb_method_t` already found and does the rest of
  `mrb_funcall_with_block` (frame push, argument capture, call) removes
  the second search. It holds no cache: the search and the call sit in
  one C statement, no VM instruction and no GC runs between them, so
  nothing can invalidate the result. An mruby proposal.
- A cached answer for "is this method still the builtin", so a guarded
  helper tests a flag instead of a search. The flag needs invalidation
  when a method table changes. mruby already solves the same problem for
  the index opcodes: the `idx_class` and `bop` slots stay armed while
  the name resolves to the builtin recorded at startup, and every place
  that changes a method table (def, alias, undef, remove, visibility,
  include, prepend) calls `mrb_builtin_op_update(mrb, mid)` to resolve
  them again. Three ways to reuse that:
  - Register (class, mid, cfunc) tuples as AOT slots at gem init and
    resolve them again from the same hook. Correct by the same argument
    as the opcodes, but each `def` at boot rechecks every slot with the
    same name, and the slot count grows with the number of guarded call
    sites in all gems.
  - A global `method_epoch` counter in `mrb_state`, incremented at the
    same hook sites and where the GC frees a class. Each call site keeps
    a static (class, epoch, answer) and skips the search while both
    match. The smallest mruby change, and correct by one rule. Any `def`
    anywhere invalidates every site, which costs one search each on the
    next call and nothing more; a device defines almost no methods after
    boot. The statics add about 12 bytes of RAM per call site.
  - A bit per class (the first form of this idea). Harder than it looks:
    the receiver's class is often a subclass of the class that owns the
    cfunc, and an override on the subclass, an `include` into an
    ancestor or a `def` on a shared module must all reach the receiver's
    bit. `RClass` keeps no list of its subclasses, so there is no way to
    propagate the change. The epoch counter is the practical form.

  Preferred: the epoch counter, when a profile shows the guard searches
  above the 7 to 12 percent measured so far.
- `mrb_funcall_argv_kw(mrb, self, mid, argc, argv, kw_hash)`: a funcall
  that sets the frame's keyword flag, so a call with keywords from C
  needs no Ruby trampoline. An mruby proposal.
- `SystemStackError#message` prints the class name, not "stack level
  too deep" (mruby follow-up).
- Spinel: rest parameter typing (1.3).

## 2. Candidate gems

The gems the owner wants compiled that do not declare `spec.aot` yet.
Counts are typed / widened of the defs the generator sees, from
`tools/aot-gen --include-widened`. Before a gem is declared it needs a
picotest suite the twin can run (each of these has one) and a device
measurement of the path that matters.

| Gem | Compiled | Blocked by | Expected gain |
|---|---|---|---|
| picoruby-regexp | 2 / 5 of 11 | rest parameters on the String wrappers (1.3) | Low: the engine is C; the wrappers are thin |
| picoruby-vfs | 28 / 45 of 71 | rest parameters on `gets`, `printf`, `chmod`, `utime` (1.3); `Dir.open`, `VFS.sanitize` and more named at load (`alias`, `private :x`, `VFS.pwd`; 1.4); `load_file` waits through `f.read` on the block parameter (1.4); `VFS.split`, `sanitize_and_split`, `File.dirname` widened (a tuple return) | Moderate: the file I/O is C, the path and mode logic is Ruby |
| picoruby-vim | 1 / 1 of 6 | every def waits through editor's `load_file_into_buffer`, `save_file_from_buffer` (`File.open` in a dependency, 1.4) or `getch` | Low here, high through editor, which is compiled already |
| picoruby-vram | 5 / 5 of 5 | nothing | Low: `VRAM` itself is C; `Delegatable` forwards |
| picoruby-yaml | 5 / 11 of 13 | `load`, `dump`, `parse`, `serialize`, `parse_value` and the two split helpers are widened (a YAML value is any type); `load_file` waits through `File.open` in a dependency (1.4); a bare `raise` in `is_integer?` (1.2) | Low: the parser is line-based on C String methods |

The measurements that keep them undeclared are in section 3.

## 3. Decided against

- **A gem is declared only when a measurement shows a gain.** A
  compiled gem costs code size (funicular's wasm 2141863 -> 2397546 B;
  on an RP2040 or RP2350 the flash budget is the limit), and a gem whose
  hot path is C or the JS bridge (picoruby-wasm: `JS::Object#[]`,
  `method_missing`, `respond_to?` are C) gains nothing from it. No
  declaration for consistency's sake, and no declaration for a gem
  that merely passes the twin.
- **vfs, vram, vim and yaml are not declared.** Measured on pico2_w
  (prod, one irb line, the median of three runs): `File.expand_path` +
  `File.dirname` x200 ran 0.88x, because the compiled def's work is one
  call into `VFS.sanitize`, which stays in the irep, and a C-to-VM call
  costs more than a VM-to-VM one. `VRAM::Delegatable#set_pixel` x10000
  ran 1.03x and `draw_rect` x2000 1.04x: `Delegatable` is a bounds check
  and a call into C `VRAM`, which the VM dispatches as cheaply. vim's
  hot loop (`_start`) has a block literal, and the four defs that
  compile run once per command. `YAML.load` of the README document
  x200 ran 1.19x and `YAML.dump` 1.14x for 6.6 KB of text: the parser's
  work per line is `split`, `strip`, `start_with?` and Hash and Array
  operations, all C builtins the generated code calls as the VM does.
  Any of them is declared again when a device profile shows a Ruby hot
  path in it.
- **No RBS or seed route around a polymorphic parameter.** Spinel
  widens a parameter that takes more than one kind of value
  (`Marshal.dump(obj)`) whatever the RBS says. Tried on marshal: an RBS
  overload (`| ...`, which steep requires beside the core declaration,
  folded away for Spinel), a seed file of calls fed to Spinel only, and
  calls of one kind, of several kinds, of Array and Hash, and of an
  untyped value. Every form leaves `dump`, `dump_object` and the
  `load_*` family widened, and a call of one kind narrows the parameter
  to it. `spec.aot_include_widened` is the way, and marshal declares
  it. A Spinel proposal that a `| ...` with no base declaration in the
  given directories be taken as the declaration is possible but of
  low value.
- **Marshal's CRuby arity** (`dump(obj, io = nil, limit = -1)`) does not
  help AOT: the duplicate-definition error of the RBS route does not
  depend on arity, and Spinel reads no core RBS. It would only remove
  a `steep:ignore`. A separate API change, if at all.
- **No work on a def that may wait for the scheduler.** mruby-task
  switches VM contexts between instructions, never under a C frame; a
  compiled loop around `sleep_ms` would spin. Such a def and everything
  that calls it stay in the irep, and no generator work changes that.

## 4. How to keep this document current

- After a generator change: run `rake test:gems:aot_twin` and
  `rake test:gems:aot_twin32`, then refresh the counts in section 2
  (`SPINEL=mrbgems/picoruby-aot/lib/spinel/bin/spinel ruby
  mrbgems/picoruby-aot/tools/aot-gen --include-widened --gem GEM ...
  --manifest FILE`). Move a done item to `architecture.md`.
- After a semantics change: update `architecture.md` section 4.
- After a device measurement: add the number to `architecture.md`
  section 5 for a declared gem, or to section 3 here for a decision
  against.
- When a gem is declared with `spec.aot`: remove its row from section 2
  and add it to `architecture.md` section 2.
