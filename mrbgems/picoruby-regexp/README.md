# picoruby-regexp

Regular expressions for PicoRuby and FemtoRuby. The gem carries a regex engine
imported from its upstream repository: two files, `lib/regex_engine.c` and
`lib/regex_engine.h`. The bindings for mruby and mruby/c live in `src/`.

## The engine

The engine compiles a pattern to a small program. A Pike VM (a Thompson NFA
simulation that carries capture positions) runs the program.

- A match costs O(input x program) time. A pattern such as `(a*)*b` runs in
  linear time.
- The VM does not recurse and does not allocate. Each `Regexp` holds one
  scratch block, sized at compile time.
- The compiler grows its buffers with the pattern. `/a/` compiles in about 1 KB
  of temporary heap.
- Text is UTF-8. `.` and a class consume one character. `MatchData#begin`,
  `MatchData#end`, `Regexp#=~` and the `pos` argument count characters.

## API

| Class | Methods |
|-------|---------|
| `Regexp` (class) | `compile(pattern, options = nil, encoding = nil)`, `new` (the same), `escape(str)` |
| `Regexp` | `match(str, pos = 0)`, `match?(str, pos = 0)`, `===`, `=~`, `source`, `to_s`, `inspect`, `casefold?`, `options` |
| `MatchData` | `[]`, `to_a`, `captures`, `length`, `size`, `string`, `regexp`, `pre_match`, `post_match`, `begin(n)`, `end(n)`, `byteoffset(n)`, `to_s`, `inspect` |
| `String` | `match(pattern, pos = 0)`, `match?(pattern, pos = 0)`, `=~`, `sub`, `sub!`, `gsub`, `gsub!`, `scan`, `split` |

`options` is a String of letters (`"im"`) or an Integer built from
`Regexp::IGNORECASE`, `Regexp::EXTENDED` and `Regexp::MULTILINE`. The
`encoding` argument is accepted and ignored.

The `String` methods take a Regexp or a String as the pattern. `match`,
`match?` and `=~` compile a String pattern as a regular expression. `sub`,
`gsub`, `scan` and `split` take a String pattern as literal text, as CRuby
does. `split` with a String or nil pattern keeps the VM's own behavior.

`sub` and `gsub` take a replacement String, a Hash or a block. A replacement
String expands `\0`, `\&`, `\1` to `\9` and `\\`. The block receives the
matched text. The receiver must not change while the block runs, or the
method raises `RuntimeError`. `gsub` without a replacement and without a
block raises `ArgumentError`; it does not return an Enumerator.

`scan` returns the matched texts, or Arrays of the groups when the pattern has
groups. `split` puts the groups of each match into the result, drops trailing
empty pieces when `limit` is 0 and keeps them when `limit` is negative.

On mruby/c the block forms run in Ruby (`mrblib/regexp.rb`) over byte
offsets that the C side reports, because C code cannot call a block there.
Everything else runs in C on both VMs.

A pattern the engine does not accept raises `RegexpError`. The message names
the reason and the byte offset in the pattern.

## Syntax

| Syntax | Meaning |
|--------|---------|
| `.` | Any character. A newline only with `m` |
| `[...]`, `[^...]` | Class and negated class. Ranges span code points, so `[あ-ん]` works |
| `\d` `\w` `\s` `\D` `\W` `\S` | ASCII digit, word and space classes and their negations |
| `\n` `\t` `\r` `\f` `\v` `\e` `\0` | Control characters |
| `\.` and other escaped punctuation | The literal character |
| `*` `+` `?` `{n}` `{n,}` `{n,m}` | Greedy repetition. `n` and `m` go up to 256 |
| `*?` `+?` `??` `{n,m}?` | Lazy repetition |
| `(...)` | Capture group. Up to 15 groups |
| `(?:...)` | Group without capture |
| `\|` | Alternation. The earlier alternative wins at the same position |
| `^` `$` `\A` `\z` `\Z` | Start and end of the string |

## Options

| Option | Effect |
|--------|--------|
| `i` | Case folding for ASCII letters only |
| `m` | `.` also matches a newline |
| `x` | Kept in `options`, but the pattern is taken as written |

## Differences from CRuby

- `^` and `$` anchor the whole string, not a line. `\Z` is the same as `\z`.
- `i` folds ASCII letters only. `/é/i` does not match `É`.
- These raise `RegexpError` at compile time: backreferences (`\1`), word
  boundaries (`\b`), lookahead and lookbehind, named groups, possessive
  quantifiers, `\x`, `\u`, `\p{...}` and POSIX classes.
- `$~`, `$1` and the other match globals are not set.
- `sub` and `gsub` do not expand `` \` ``, `\'` or `\k<name>`.
- `String#index`, `rindex`, `[]`, `slice`, `start_with?` and `partition` do not
  take a Regexp.

## Example

```ruby
md = /(\w+)\s(\w+)/.match("hello world")
md[0]        # => "hello world"
md.captures  # => ["hello", "world"]

/\A\d+\z/.match?("123")        # => true
"abc 42 def" =~ /\d+/          # => 4
/./.match("あい", 1)[0]        # => "い"

case "2024-01-15"
when /\A\d{4}-\d{2}-\d{2}\z/ then :date
when /\A\d+\z/               then :number
else                              :other
end
# => :date
```

## Updating the engine

Run the import script with the upstream runtime directory:

```
ruby lib/import.rb <upstream runtime directory>
```

The script writes `lib/regex_engine.c` and `lib/regex_engine.h`. It drops the
upstream class glue, renames the public identifiers to the `regex_` prefix and
rewrites the comments that describe the upstream runtime. It fails when a
name of the upstream project survives. Do not edit the two files by hand. The
engine calls `regex_malloc`, `regex_realloc` and `regex_free`, which
`src/regexp.c` supplies.
