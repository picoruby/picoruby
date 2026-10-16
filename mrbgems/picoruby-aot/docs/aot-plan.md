# picoruby-aot plan

This document lists the work to do, the work that may be done, and the
decisions against doing something. What picoruby-aot does today is in
`architecture.md`. When an item here is done, it moves there and leaves
this document. The aim is a small document of decisions, which then
merges into `architecture.md`.

## 1. Work to do

Ordered by the ratio of defs gained to design needed.

### 1.1 A block literal at a call site

A def that passes a block (`File.open(path) { |f| ... }` in most of
them) stays in the irep, because the generated C would have to make the
Proc. The sweep over every gem counts 57 such defs; 24 of them are in
the host build.

How Spinel does it: a block is never a closure by default. A builtin
iterator becomes a C `for` loop with the block body inside and the
block parameters as method locals; a Ruby method that yields is inlined
into its caller; `next` is `continue`, `break v` a `goto` out of the
loop, `return` the method's C return. Only a block that is stored or
passed on becomes a C function with a capture struct, and a captured
local the block writes becomes a GC cell both sides read through.
Spinel owns the whole program. picoruby-aot shares the VM, and mruby
has no `break` or method `return` from a cfunc Proc (nothing in the C
API raises the VM's break).

Options, in the order to try:

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

### 1.2 `begin`/`rescue`/`ensure`

A rescue in C needs `mrb_protect_error` or `mrb_rescue_exceptions`
around the body as a nested C function, with the locals the body
assigns passed by reference; `ensure` needs `mrb_ensure`. The body
inside the closure cannot `return` from the method directly. Decide
first whether to compile only the forms without `return`/`break`
inside the protected body. About 30 defs, mostly net and midibase. A
bare `raise` in a rescue needs this too.

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

### 1.6 picoruby-shell and picoruby-funicular

The owner's next targets. Both pass Spinel now: funicular since its
RBS fix (`render_text` returns `JS::Object`), shell since
picoruby-require defines `$LOAD_PATH = []` and the shell changes it in
place (Spinel takes `$LOAD_PATH` as read-only, as CRuby does). What is
left before either can declare:

- shell has no picotest suite; the twin needs one before the gem can
  declare.
- funicular: the types. On wasm a JS call may suspend the task
  (`JS::Promise#await`, `IndexedDB::Store#[]`), and the wait search
  takes a call on an untyped receiver for every def of its name, so an
  untyped `x[]` or `x.each` keeps its caller in the irep. The wasm twin
  found the two shapes that ran wrong when compiled (an `await` under a
  cfunc returns at once; the `included` hook) and the search is sound
  now, at 117 typed / 227 widened of 747 on the wasm build, `VDOM.diff`,
  `VDOM.patch` and `Renderer#render` in the irep. What is left behind
  the waits, by their roots (the first call of each excluded def that
  is a primitive or resolves to another gem's def):
  - A Hash value or an Array element Spinel can only box
    (`@messages.each { |attr, msgs| msgs.each }`: `msgs` is poly) on
    about 50 roots, which then meet `IndexedDB::Store#each`, `#empty?`,
    `#keys`, `#delete`. Spinel has no typed-value Hash beyond `int` and
    `string` values, so the RBS (`Hash[Symbol, Array[String]]`) cannot
    say more. A Spinel topic, not a gem one.
  - A `Hash[Symbol, Integer]` seed fell to poly in Spinel (its parser
    had no such kind); fixed upstream (matz/spinel PR #9315), and the
    submodule carries it. It typed the ivars, and the def count did
    not move: the waits behind them have other roots (above).
  - The same collision reaches marshal on the wasm build: `dump_hash`
    calls `keys` on its untyped object, which could be
    `IndexedDB::Store#keys` (an await), so marshal's dump path is in
    the irep there (14/19 typed on wasm, 18/19 on the host and the
    device, which carry no IndexedDB).
  - A class-valued receiver (`component_vnode.component_class.new`,
    RBS `singleton(Component)`, Spinel `class`) reaches every
    `initialize`; the record's `rbs` string could narrow it.
  - A `Hash[Integer, ...]` seed is dropped by the extractor (no kind).
- funicular, after the types: a browser measurement of the VDOM path,
  then the `while` rewrite of the hot-path defs a block literal keeps
  in the irep (1.1, option 3; about 15 defs in vdom, differ, patcher
  and html_serializer), or 1.1 option 2 in the generator.

The generator compiles 38 of 79 shell defs (the Tokenizer, the Parser,
PipeIO; the rest wait for the scheduler through `exec`, or take
`*rest`). The shell's compiled defs run once per command line, so a
device measurement decides whether shell declares at all.

### 1.5 Beyond the generator

Two ideas cut the method searches that the generated C makes. Measure
first: the device build uses `MRB_CONSTRAINED_BASELINE_PROFILE`, which
defines `MRB_NO_METHOD_CACHE`, so on the device every search walks the
ancestor chain and its khash tables. Profile a hot gem on the device and
look for `mrb_method_search_vm`. If it does not show, do neither.

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

  Preferred: the epoch counter, after the profile shows the search.
- `mrb_funcall_argv_kw(mrb, self, mid, argc, argv, kw_hash)`: a funcall
  that sets the frame's keyword flag, so a call with keywords from C
  needs no Ruby trampoline. An mruby proposal.
- `SystemStackError#message` prints the class name, not "stack level
  too deep" (mruby follow-up).
- A gem init that raises is swallowed by `mrb_protect_error` in
  `init_mrbgems_body`, and every later gem stays uninitialized; the
  first symptom is `require` answering false. A loud failure at init
  would be a PicoRuby or mruby change worth making.
- Spinel: rest parameter typing (1.3).

## 2. Candidate gems

The gems the owner wants compiled that do not declare `spec.aot` yet.
Counts are typed / widened of the defs the generator sees, from
`tools/aot-gen --include-widened`. Before a gem is declared it needs a
picotest suite the twin can run (shell has none; the others have one)
and a device measurement of the path that matters.

| Gem | Compiled | Blocked by | Expected gain |
|---|---|---|---|
| picoruby-regexp | 2 / 4 of 11 | rest parameters on the String wrappers (1.3), a block literal in `__sub_common` (1.1) | Low: the engine is C; the wrappers are thin |
| picoruby-vfs | 24 / 42 of 71 | rest parameters on `gets`, `printf`, `chmod`, `utime` (1.3); block literals in `Dir.glob` and `load_file` (1.1); `File.open`, `Dir.open`, `VFS.sanitize` and more named at load (`alias`, `private :x`, `VFS.pwd`; 1.4); `VFS.split`, `sanitize_and_split`, `File.dirname` widened (a tuple return) | Moderate: the file I/O is C, the path and mode logic is Ruby |
| picoruby-vim | 3 / 4 of 6 | block literals in `_start` and `initialize` (1.1) | Low here, high through editor, which is compiled already |
| picoruby-vram | 5 / 5 of 5 | nothing | Low: `VRAM` itself is C; `Delegatable` forwards |
| picoruby-yaml | 5 / 11 of 13 | `load`, `dump`, `parse`, `serialize`, `parse_value` and the two split helpers are widened (a YAML value is any type); a block literal in `load_file` (1.1); a bare `raise` in `is_integer?` (1.2) | Low: the parser is line-based on C String methods |
| picoruby-shell | 19 / 38 of 79 | no picotest suite (1.6); 18 defs wait through `exec`; `*rest` on 10 (1.3); rescue on 5 (1.2) | Unknown: the compiled defs (Tokenizer, Parser, PipeIO) run once per command line; measure the shell first |
| picoruby-funicular | 117 / 227 of 747 | waits through untyped receivers on about 480 (1.6: the types); block literals (1.1), adjacent interpolated strings (1.4), rescue (1.2), `*rest` and `**rest` (1.3) behind them | Unknown: VDOM diff, patch and render are in the irep until the types come; then a browser measurement |

The measurements that keep them undeclared are in section 3.

## 3. Decided against

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
