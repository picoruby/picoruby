# PicoRuby Development Guidelines

This guideline applies only to implementing libraries (gems): not application code, tests, or CRuby tools.

## Submodules

PicoRuby contains many submodules such as `mrbgems/mruby-compiler` and `mrbgems/picoruby-mruby/lib/mruby`.

Generally, you should not modify submodules. If modifying a submodule is technically justifiable, you must report it and ask for instructions, regardless of the coding mode or setting.

## Implementing a library (gem)

### Iteration

Ruby library implementations should avoid block-based iteration where practical.
Methods such as `loop`, `each`, `each_*`, `times`, `map`, and `select` create
ireps and deepen the VM stack on constrained targets. Prefer `while` loops with
an explicitly managed integer index.

Design internal collections for indexed traversal where possible. If a
block-based iterator is necessary in library code, add a short comment that
explains why indexed iteration is not practical.

Apply this rule to new and modified library implementation code. Tests,
examples, build scripts, and user applications are exempt.

### Memoization

As in, calling the same ivar repeatedly costs. If it'll be 3+ times, memoize like `var = @var`. lvar costs much less.

The example below contains a rather higher cost of calling `Array#size`:

```ruby
i = 0
while i < @ary.size
  # do something with @ary[i]
  i += 1
end
```

```ruby
i = 0
ary = @ary
ary_size = ary.size
while i < ary_size
  # do something with ary[i]
  i += 1
end
```

Note that if there is a possibility of modifying `@ary` in another context, memoization could introduce a bug.

