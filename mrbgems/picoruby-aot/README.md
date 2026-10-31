# picoruby-aot

picoruby-aot compiles typed methods of a gem's `mrblib` into mruby cfuncs
at build time. The VM, the irep and irb stay as they are. A compiled
method is an ordinary C method of its class.

Spinel (the Ruby AOT compiler) runs on the host as a type oracle. It
reads the gem's Ruby and the RBS files of its dependencies and writes a
type for every node. picoruby-aot reads those types and the Prism AST,
and writes C against the mruby API. Spinel's own C and runtime are not
used.

- `docs/architecture.md`: how it works today, which defs compile, how
  the generated C behaves, how it is verified.
- `docs/aot-plan.md`: the work to do, the candidate gems, and the
  decisions against.

## Use

A gem declares in its `mrbgem.rake` that it is to be compiled:

```ruby
spec.aot = true                 # the typed methods
spec.aot_include_widened = true # the widened ones as well
spec.aot_exclude = %w[PSG::Driver#debug_dump]   # these stay in the irep
```

A target turns the machinery on. Without `conf.aot` a declaration
changes nothing:

```ruby
conf.aot                                     # the Spinel submodule, built in place
conf.aot path: "/path/to/spinel"             # an explicit binary
conf.gem(core: 'picoruby-json') { |g| g.aot = false }   # opt out
```

Spinel is the submodule `lib/spinel` of this gem. Initialize it once:

```
git submodule update --init --depth 1 mrbgems/picoruby-aot/lib/spinel
```

The first build runs `make deps` and `make` there. The R2P2 configs
build with AOT when `PICORB_AOT=1` is set. The build writes the decision
and the reason for each def to
`build/<target>/mrbgems/<gem>/aot/manifest.json`.

## Twin test

The same picotest suite runs on the irep build, the AOT build and the
AOT build with the widened defs; the summaries must match:

```
rake "test:gems:aot_twin[picoruby-json]"
rake test:gems:aot_twin      # every gem
rake test:gems:aot_twin32    # the 32-bit host build, where a Bignum shows
rake test:gems:aot_twin_wasm # the wasm build, under the Node runner
```

CI runs all three. See `docs/architecture.md` section 5.

## Layout

```
include/picoruby_aot.h   helpers the generated C calls
src/picoruby_aot.c       their implementation (linked into libmruby)
mrblib/picoruby_aot.rb   PicoRubyAOT.send_kw
tools/                   host-side generator (Ruby, needs the prism and rbs gems)
lib/spinel               the Spinel submodule
docs/                    architecture.md, aot-plan.md
```
