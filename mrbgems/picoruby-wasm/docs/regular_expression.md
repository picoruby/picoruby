# Regular Expressions in PicoRuby.wasm

PicoRuby.wasm provides `Regexp` and `MatchData` classes by wrapping JavaScript's `RegExp` engine.
This is a wasm-only feature; it is not available on microcontroller targets.

## Overview

The compiler translates regexp literals (`/pattern/flags`) into `Regexp.compile(pattern, flags_str)` calls.
The runtime creates a JS `RegExp` object under the hood, so the actual matching behavior follows
the ECMAScript specification, not Onigmo (CRuby's engine).

## Basic Usage

```ruby
# Literal syntax
re = /(\w+)\s(\w+)/i
md = re.match("Hello World")
md[0]        #=> "Hello World"
md[1]        #=> "Hello"
md[2]        #=> "World"
md.captures  #=> ["Hello", "World"]

# Constructor
re = Regexp.new("hello", "i")
re.match?("HELLO")  #=> true
re =~ "say hello"   #=> 4

# String methods
"foo123bar".match(/(\d+)/)[1]  #=> "123"
"hello" =~ /ell/               #=> 1
```

## Flag Mapping

Ruby flags are translated to JavaScript `RegExp` flags:

| Ruby flag | Meaning               | JS flag | Notes |
|-----------|-----------------------|---------|-------|
| `i`       | Case-insensitive      | `i`     | Direct mapping |
| `m`       | MULTILINE (`.` matches `\n`) | `s` | Ruby `m` = JS dotAll `s` |
| `x`       | Extended (free-spacing) | _(none)_ | Silently accepted; JS has no equivalent |

Additionally, the JS `m` (multiline) flag is **always enabled** so that `^` and `$` match
line boundaries, matching default Ruby behavior.

### Integer flags

`Regexp.new` also accepts an integer as the second argument:

| Constant             | Value |
|----------------------|-------|
| `Regexp::IGNORECASE` | 1     |
| `Regexp::EXTENDED`   | 2     |
| `Regexp::MULTILINE`  | 4     |

```ruby
re = Regexp.new("hello", Regexp::IGNORECASE | Regexp::MULTILINE)
```

## Regexp API

| Method | Return | Description |
|--------|--------|-------------|
| `Regexp.compile(pattern, flags=nil)` | Regexp | Create from string; called by literal syntax |
| `Regexp.new(pattern, flags=nil)` | Regexp | Alias for `compile` |
| `Regexp#match(str)` | MatchData or nil | Execute match |
| `Regexp#match?(str)` | true/false | Test without creating MatchData |
| `Regexp#=~(str)` | Integer or nil | Match index |
| `Regexp#source` | String | Pattern string |
| `Regexp#to_s` | String | `"(?flags:source)"` |
| `Regexp#inspect` | String | `"/source/flags"` |
| `Regexp#casefold?` | true/false | Whether `i` flag is set |
| `Regexp#options` | Integer | Bitmask of flag constants |

## MatchData API

| Method | Return | Description |
|--------|--------|-------------|
| `MatchData#[](n)` | String or nil | Nth capture (0 = full match) |
| `MatchData#to_a` | Array | All captures including full match |
| `MatchData#captures` | Array | Captures only (excludes `[0]`) |
| `MatchData#length` / `#size` | Integer | Number of elements |
| `MatchData#to_s` | String | Full match string |
| `MatchData#string` | String | Original string (frozen) |
| `MatchData#regexp` | Regexp | The Regexp that produced this match |
| `MatchData#pre_match` | String | Substring before the match |
| `MatchData#post_match` | String | Substring after the match |
| `MatchData#begin(n)` | Integer or nil | Start byte position of nth capture |
| `MatchData#end(n)` | Integer or nil | End byte position of nth capture |
| `MatchData#inspect` | String | Human-readable representation |

## String Extensions

| Method | Return | Description |
|--------|--------|-------------|
| `String#match(regexp_or_str)` | MatchData or nil | Match against Regexp or pattern string |
| `String#match?(regexp_or_str)` | true/false | Test without creating MatchData |
| `String#=~(regexp)` | Integer or nil | Match index |

## Differences from CRuby

- **Engine**: JavaScript RegExp (ECMAScript), not Onigmo.
- **Named captures**: JS supports `(?<name>...)`, but `MatchData` does not provide `#named_captures` or symbol key access.
- **`$~`, `$1`..`$9`**: Global match variables are not set.
- **`x` flag**: Accepted but has no effect (no comment/whitespace stripping).
- **Encoding argument**: A third argument to `Regexp.compile` is accepted but ignored.
- **Lookbehind**: JS supports lookbehind (`(?<=...)`, `(?<!...)`); CRuby also does, but syntax details may differ.
- **Unicode properties**: JS `\p{...}` requires the `u` flag; this implementation does not add `u` automatically.

## Implementation

The implementation lives in:

- `src/regexp.c` -- VM dispatcher
- `src/mruby/regexp.c` -- EM_JS bindings and mruby class definitions

Regexp is a synchronous operation; no task suspension or async machinery is involved.
Each `Regexp` and match result is stored as a JS object reference in `globalThis.picorubyRefs`.

## See Also

- [architecture.md](architecture.md) -- Task-based execution model
- [interoperability_between_js_and_ruby.md](interoperability_between_js_and_ruby.md) -- JS/Ruby type conversion
