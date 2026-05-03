# Regular Expressions in PicoRuby.wasm

PicoRuby.wasm provides `Regexp` and `MatchData` classes by wrapping JavaScript's `RegExp` engine.
This is a wasm-only feature; it is not available on microcontroller targets.

## Overview

The compiler translates regexp literals (`/pattern/flags`) into `Regexp.compile(pattern, flags_str)` calls.
The runtime creates a JS `RegExp` object under the hood, so the actual matching behavior follows
the ECMAScript specification, not Onigmo (CRuby's engine).

mruby strings in this build are UTF-8 (`MRB_UTF8_STRING`). Indices returned by
`MatchData#begin`/`#end`, `Regexp#=~` and `String#=~` are **Unicode code-point
(character) offsets** -- consistent with how `String#[]` and `String#length`
behave on UTF-8 strings -- so they can be used directly to slice the original
string. `MatchData#pre_match` and `#post_match` similarly return substrings cut
at code-point boundaries.

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
line boundaries, matching default Ruby behavior. The JS `u` (Unicode) and `d`
(hasIndices) flags are also auto-added so that `.` / character classes /
`\p{...}` operate on Unicode code points, and per-capture offsets are accurate
even when the same substring appears multiple times.

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
| `Regexp#match(str, pos=0)` | MatchData or nil | Execute match. `pos` is a character (code-point) offset; negative `pos` raises ArgumentError; out-of-range returns nil |
| `Regexp#match?(str, pos=0)` | true/false | Test without creating MatchData; same `pos` semantics |
| `Regexp#=~(str)` | Integer or nil | Match position as a character offset |
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
| `MatchData#pre_match` | String | Substring before the match (cut at a UTF-8 code-point boundary) |
| `MatchData#post_match` | String | Substring after the match (cut at a UTF-8 code-point boundary) |
| `MatchData#begin(n)` | Integer or nil | Start character (code-point) offset of nth capture |
| `MatchData#end(n)` | Integer or nil | End character (code-point) offset of nth capture |
| `MatchData#inspect` | String | Human-readable representation |

## String Extensions

| Method | Return | Description |
|--------|--------|-------------|
| `String#match(regexp_or_str, pos=0)` | MatchData or nil | Match against Regexp or pattern string. `pos` is a character (code-point) offset |
| `String#match?(regexp_or_str, pos=0)` | true/false | Test without creating MatchData; same `pos` semantics |
| `String#=~(regexp)` | Integer or nil | Match position as a character offset |
| `String#sub(pattern, replacement)` | String | Replace the first match. `pattern` may be a Regexp or a String literal pattern |
| `String#sub(pattern) { \|m\| ... }` | String | Block form; the block result (`to_s`-coerced) replaces the match |
| `String#gsub(pattern, replacement)` | String | Replace all matches |
| `String#gsub(pattern) { \|m\| ... }` | String | Block form |

In the replacement-string form of `sub`/`gsub`, the following special sequences
are expanded:

| Sequence  | Expands to |
|-----------|-----------|
| `\&`, `\0` | The full match |
| `\1` .. `\9` | The nth capture group |
| `\\` | A literal backslash |

Other backslash sequences (e.g. `\x`) are left as-is. The `\\&` placeholder
inside a Ruby string literal corresponds to `\&` in the replacement string;
double the backslashes when copying examples from Ruby source. Note: named
back-references (`\k<name>`) are not yet supported.

## Differences from CRuby

- **Engine**: JavaScript RegExp (ECMAScript), not Onigmo.
- **Named captures**: JS supports `(?<name>...)`, but `MatchData` does not provide `#named_captures` or symbol key access.
- **`$~`, `$1`..`$9`**: Global match variables are not set.
- **`x` flag**: Accepted but has no effect (no comment/whitespace stripping).
- **Encoding argument**: A third argument to `Regexp.compile` is accepted but ignored.
- **Lookbehind**: JS supports lookbehind (`(?<=...)`, `(?<!...)`); CRuby also does, but syntax details may differ.
- **Unicode properties**: This implementation auto-adds the `u` flag, so `\p{...}` is always available. However, JS RegExp uses ECMAScript property syntax, which differs from Onigmo:
    - General Category shorthand works the same: `\p{Letter}`, `\p{Number}`.
    - **Script values must be qualified**: write `\p{Script=Hiragana}` or `\p{sc=Hiragana}`. Onigmo's `\p{Hiragana}` shorthand is **not** accepted by JS RegExp.
- **Replacement strings**: `sub`/`gsub` support `\0`/`\&`, `\1`..`\9`, and `\\`. Named back-references (`\k<name>`) and Ruby's `\'`/`\``/`\+` are not supported.
- **Enumerator form**: `sub`/`gsub` without a block or replacement raises ArgumentError; they do not return an Enumerator.

## Implementation

The implementation lives in:

- `src/regexp.c` -- VM dispatcher
- `src/mruby/regexp.c` -- EM_JS bindings and mruby class definitions

Regexp is a synchronous operation; no task suspension or async machinery is involved.
Each `Regexp` and match result is stored as a JS object reference in `globalThis.picorubyRefs`.

## See Also

- [architecture.md](architecture.md) -- Task-based execution model
- [interoperability_between_js_and_ruby.md](interoperability_between_js_and_ruby.md) -- JS/Ruby type conversion
