# picoruby-regexp_light

Lightweight regular expression engine for PicoRuby (microcontrollers).
Wraps the [regex_light](lib/regex_light) C library with mruby and mrubyc bindings.

**ASCII only. UTF-8 is not supported.**

## Features

- Regexp literal syntax: `/pattern/flags`
- `Regexp.compile` / `Regexp.new`
- `Regexp#match` -> `MatchData` or `nil`
- `Regexp#match?` -> `true`/`false`
- `Regexp#===` -> `true`/`false` (for `case`-`when`)
- `Regexp#=~` -> match index or `nil`
- `MatchData`: `[]`, `to_a`, `length`, `captures`, `pre_match`, `post_match`, `begin`, `end`
- `String#match`, `String#match?`, `String#=~`

## Syntax supported

| Syntax | Description |
|--------|-------------|
| `.`    | Any character |
| `?`    | Zero or one (greedy) |
| `*`    | Zero or more (greedy) |
| `+`    | One or more (greedy) |
| `{n}`  | Exactly n repetitions |
| `{n,m}` | Between n and m repetitions (greedy) |
| `{n,}` | n or more repetitions (greedy) |
| `^`    | Start of string |
| `$`    | End of string |
| `\A`   | Start of string (converted to `^`) |
| `\z` / `\Z` | End of string (converted to `$`) |
| `(...)` | Capture group |
| `[...]` | Character class |
| `[a-z]` | Character range |
| `\w`   | Word character `[a-zA-Z0-9_]` |
| `\s`   | Whitespace `[ \t\f\r\n]` |
| `\d`   | Digit `[0-9]` |

## Flags

| Flag | Effect |
|------|--------|
| `i`  | Case-insensitive (flag stored, not implemented in matching engine) |
| `m`  | Multiline (flag stored, not implemented in matching engine) |
| `x`  | Extended (silently ignored) |

> **Note**: The underlying `regex_light` library does not implement case-insensitive
> or multiline matching. The `i` and `m` flags are accepted for API compatibility but
> have no effect on actual matching behaviour.

## Not supported

- Alternation `|`
- Non-greedy quantifiers `*?`, `+?`, `??`
- Lookahead / lookbehind
- Named captures `(?<name>...)`
- UTF-8 / multibyte characters
- `$~`, `$1`, ... global variables

## Memory note (mrubyc / PicoRuby)

`Regexp` objects allocate memory for the compiled pattern (`atoms`) via `malloc`.
This memory is freed automatically in the mruby (MicroRuby) build via a GC finalizer.

In the mrubyc (PicoRuby) build there is no GC finalizer. Call `regexp.free` explicitly
if you need to reclaim the atoms memory:

```ruby
re = /\d+/
# ... use re ...
re.free
```

## Example

```ruby
# Basic match
md = /(\w+)\s(\w+)/.match("hello world")
md[0]        # => "hello world"
md[1]        # => "hello"
md.captures  # => ["hello", "world"]

# Anchors
/\A\d+\z/.match?("123")   # => true
/\A\d+\z/.match?("12x")   # => false

# Route constraint use-case
constraint = /\A\d+\z/
constraint.match?("42")    # => true
constraint.match?("slug")  # => false

# String extension
"abc 42 def" =~ /\d+/      # => 4

# case-when with ===
case "2024-01-15"
when /\A\d{4}-\d{2}-\d{2}\z/ then :date
when /\A\d+\z/                then :number
else                               :other
end
# => :date
```
