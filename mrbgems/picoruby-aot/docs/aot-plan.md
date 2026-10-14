# picoruby-aot: what compiles, what does not, and the work in between

This document is the working map of picoruby-aot. It says what the
generator compiles today, what it leaves in the irep and why, and what
work each gap needs. Update it with every change to the generator or the
runtime: the tables, the counts and the gem status.

Last update: 2026-10-14, branch `picoruby-aot` at commit e0b2431a (Spinel
4983b60d).

## 1. What works

### 1.1 The pipeline

- Spinel runs on the host as the type oracle (`--emit-types`). The
  generator reads its JSON and the Prism AST and writes one C file per
  gem against the mruby API. Spinel is a submodule of this gem.
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
- Values the C holds are GC roots through the arena (see the README).

### 1.3 Verification

- The twin: the same picotest suite on the irep build, the AOT build and
  the AOT build with widened defs; the summaries must match.
  `rake test:gems:aot_twin` (every gem) is SAME on the host (99 summary
  lines, 40 gems). CI runs it.
- `rake test:gems:aot_twin32`: the twin on the 32-bit host build
  (MRB_INT32, word boxing, mruby-bigint), where a Bignum shows. SAME over
  every gem (83 lines, 34 gems; the socket gems are out on 32-bit). CI
  runs it. `picoruby-aot-probe` is the gem of typed methods the tests
  feed values past the fixnum and past `mrb_int`.
- Device (pico2_w, prod): JSON x200 12.8 s to 3.1 s (4.1x), MML x10
  2.36 s to 1.44 s (1.65x), editor `display_width` 5.9 ms to 0.64 ms
  (9.2x). ELF text +41 KB for the four gems before the 64-bit Integer,
  +10 KB more with it. RAM +1.2 KB.

### 1.4 Syntax the generator accepts

Literals (Array and Hash without a splat), locals, instance and global
variables, constants and constant paths, `if`/`unless`/`while`/`until`/
`case`, `return`, `break`, `next`, `and`/`or`/`not`, multiple assignment
to locals and instance variables, Range literals, method calls without
blocks or keywords (`&.` included), `raise` with a constant class.
Parameters: positional, required or optional.

## 2. What stays in the irep, and why

The sweep over every gem (951 defs, widened included) leaves 379 in the
irep. The reasons, largest first:

| Reason | Defs | Kind |
|---|---|---|
| Waits for the scheduler (`sleep_ms`, `submit`, `join`, ... reached through the call graph) | about 150 | Structural |
| `initialize` | 70 | Policy |
| Keyword parameters (30) and keyword arguments (14) | 44 | Generator gap |
| Blocks: a block argument (24), a block parameter (16), `yield` (3) | 43 | Generator gap, design first |
| Rest and splat parameters and arguments | 26 | Generator gap, and a Spinel issue |
| `begin`/`rescue`/`ensure` | 19 | Generator gap |
| Other (`super`, `a[i] op= x`, `a[i] ||= x`, `break` with a value, bare `raise`, a literal past 64 bits) | about 10 | Scattered |

Two gaps the sweep does not count, because the generator does not see
the defs at all:

| Gap | Where it shows |
|---|---|
| Defs inside `class << self` are skipped (the walker takes `ClassNode`, `ModuleNode` and `def self.x` only) | picoruby-marshal (all 19 defs), picoruby-vfs (about half of its defs) |
| Spinel widens a def to untyped although the RBS gives every parameter a class | `VRAM::Delegatable#set_pixel`, `#draw_line`, `#draw_rect` |

The structural reasons do not move with generator work. A cfunc cannot
yield to other tasks, so a def that may wait stays in the irep, and so
does everything that calls it. `initialize` stays by policy (it runs
once per object, and its irep is small). The reachable ceiling for the
generator is therefore about 730 of 951 defs.

Def counts are not speed. The hot paths of the declared gems are already
compiled; whether a remaining def matters shows in a device profile, not
in this table.

## 3. The work each gap needs

Ordered by the ratio of defs gained to design needed.

### 3.1 `class << self` bodies (no design)

Walk `Prism::SingletonClassNode` whose expression is `self` as a scope
whose defs are singleton methods. `private` inside it applies as in a
class body. Register with `mrb_define_class_method`. Twin: marshal, vfs.

### 3.2 Keyword parameters and arguments (little design)

- Parameters: read with `mrb_get_args` and the `:` specifier
  (`mrb_kwargs`), required and optional, in method scope for defaults
  as the optional positionals are. `**rest` stays out.
- Arguments: a call with keywords builds the Hash and sends it with the
  keyword flag (`mrb_funcall_with_block_kw` or its equivalent in the
  mruby version in use; check first). Direct impl calls between compiled
  defs pass the unboxed keyword values in order.
- Gains: vram (`draw_bitmap`, `draw_bytes`), looper, midibase, 44 defs.

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

### 3.5 Blocks (design)

Three ways, to be chosen per call shape:

1. A block whose body has no `return`, `break`, `next` with a value and
   no outer-local write becomes a C function; the call site hands it to
   the VM with `mrb_funcall_with_block` as a C-function Proc
   (`mrb_proc_new_cfunc_with_env`). Locals of the method the block reads
   travel in the env. Covers `each`, `times`, `map` over core classes.
2. A block over a core Array or Range with `each`/`times` becomes a C
   loop with the guarded element access (`aot_ary_aref`), when the
   receiver's method is still the builtin. The fastest, and the most
   generator code.
3. Leave the def in the irep and have the owner rewrite the hot loop
   with `while` (the PicoRuby style guide prefers `while` anyway).

`yield` in a compiled def takes `mrb_yield_argv` on the block the cfunc
received (`mrb_get_args "&"`). 43 defs.

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
- `SystemStackError#message` prints the class name, not "stack level
  too deep" (mruby follow-up).
- Spinel: rest parameter typing (3.3); the widening in 2 above.

## 4. The gems the owner wants compiled

Counts from `tools/aot-gen` at the commit above, with
`--include-widened`. "Blocked by" names what keeps the rest out.

| Gem | Defs the generator sees | Compiled (typed / widened) | Blocked by | Expected gain |
|---|---|---|---|---|
| picoruby-marshal | 0 of 19 | 0 / 0 | every def is inside `class << self` (3.1) | High once visible: dump/load walk bytes in Ruby |
| picoruby-regexp | 11 | 1 / 2 | rest parameters on the String wrappers (3.3), blocks in `__sub_common`, `__sub_each`, `scan` (3.5) | Low: the engine is C; the wrappers are thin |
| picoruby-uri | 7 | 4 / 6 | `initialize` only | Moderate: `parse`, `request_uri`, `encode_www_form` are string loops |
| picoruby-vfs | 36 (about 35 more inside `class << self`) | 20 / 26 | 3.1 for the class methods; rest parameters on `gets`, `write`, `puts`, `printf`, `utime` (3.3); blocks in `each`, `each_line` (3.5) | Moderate: the file I/O is C, the path and mode logic is Ruby |
| picoruby-vim | 6 | 2 / 4 | block in `_start` (3.5); `initialize` | Low here, high through editor: Vim runs on `Editor::Screen`, which is compiled already |
| picoruby-vram | 5 | 0 / 3 | keyword parameters on `draw_bitmap`, `draw_bytes` (3.2); Spinel widens `set_pixel`, `draw_line`, `draw_rect` despite the RBS (2) | Low: `VRAM` itself is C; `Delegatable` forwards |

Before any of these is declared with `spec.aot = true`, the gem needs a
picotest suite the twin can run (vfs and vim have none today) and a
device measurement of the path that matters.

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
