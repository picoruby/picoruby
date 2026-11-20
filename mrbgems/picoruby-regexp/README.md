# picoruby-regexp

Regular expressions for PicoRuby and FemtoRuby. The regex engine is two files,
`lib/regex_engine.c` and `lib/regex_engine.h`. The bindings for mruby and
mruby/c live in `src/`.

## The engine

The engine compiles a pattern to a small program. A Pike VM (a Thompson NFA
simulation that carries capture positions) runs the program.

- A match costs O(input x program) time. A pattern such as `(a*)*b` runs in
  linear time.
- The VM does not allocate, and recurses only to test a lookaround, at most
  4 levels deep. Each `Regexp` holds one scratch block, sized at compile time.
  A pattern with a backreference runs on a backtracker instead, whose stack
  comes from the heap during the search (see below).
- The compiler grows its buffers with the pattern. `/a/` compiles in about 1 KB
  of temporary heap.
- Text is UTF-8. `.` and a class consume one character. `MatchData#begin`,
  `MatchData#end`, `Regexp#=~` and the `pos` argument count characters.
- Lookahead and lookbehind (`(?=...)`, `(?!...)`, `(?<=...)`, `(?<!...)`)
  run their body as a sub-match at the current position, on a scratch level
  of their own. A lookbehind scans back at most the bounded length of its
  body. A lookahead body may scan to the end of the subject, so a pattern
  with lookahead can cost O(input) per position in the worst case; nothing
  else leaves linear time.

## API

| Class | Methods |
|-------|---------|
| `Regexp` (class) | `compile(pattern, options = nil, encoding = nil)`, `new` (the same), `escape(str)`, `quote` (the same), `union(*patterns)`, `last_match(n = nil)` |
| `Regexp` | `match(str, pos = 0)`, `match?(str, pos = 0)`, `===`, `=~`, `source`, `to_s`, `inspect`, `casefold?`, `options`, `names`, `named_captures` |
| `MatchData` | `[]`, `to_a`, `captures`, `length`, `size`, `string`, `regexp`, `pre_match`, `post_match`, `begin(n)`, `end(n)`, `byteoffset(n)`, `names`, `named_captures`, `deconstruct`, `deconstruct_keys`, `to_s`, `inspect` |
| `String` | `match(pattern, pos = 0)`, `match?(pattern, pos = 0)`, `=~`, `sub`, `sub!`, `gsub`, `gsub!`, `scan`, `split`, `index`, `rindex`, `[]`, `slice`, `slice!`, `partition`, `rpartition`, `start_with?` |
| globals | `$~`, `$&`, `$1` to `$9`, `` $` ``, `$'`, `$+` |

`options` is a String of letters (`"im"`) or an Integer built from
`Regexp::IGNORECASE`, `Regexp::EXTENDED` and `Regexp::MULTILINE`. The
`encoding` argument is accepted and ignored.

The `String` methods take a Regexp or a String as the pattern. `match`,
`match?` and `=~` compile a String pattern as a regular expression. `sub`,
`gsub`, `scan` and `split` take a String pattern as literal text, as CRuby
does. `split` with a String or nil pattern keeps the VM's own behavior.

`sub` and `gsub` take a replacement String, a Hash or a block. A replacement
String expands `\0`, `\&`, `\1` to `\9`, `\k<name>` and `\\`. The block receives the
matched text. The receiver must not change while the block runs, or the
method raises `RuntimeError`. `gsub` without a replacement and without a
block raises `ArgumentError`; it does not return an Enumerator.

`scan` returns the matched texts, or Arrays of the groups when the pattern has
groups. `split` puts the groups of each match into the result, drops trailing
empty pieces when `limit` is 0 and keeps them when `limit` is negative.

On mruby/c the block forms run in Ruby (`mrblib/regexp.rb`) over byte
offsets that the C side reports, because C code cannot call a block there.
Everything else runs in C on both VMs.

`index`, `rindex`, `[]`, `slice`, `slice!`, `partition`, `rpartition` and
`start_with?` take a Regexp.
With any other argument they behave as the VM's own methods do. On mruby/c
`rindex`, `partition` and `rpartition` are new, for a String pattern too.
`str[re, n]` takes a group number or name.

`MatchData#[]`, `begin`, `end` and `byteoffset` take a group number or a
group name as a String or Symbol. A name shared by several groups stands for
the last of them that took part in the match.

`Regexp.union` joins Strings as literal text and Regexps as the inline option
groups `Regexp#to_s` writes, so each keeps its own options. With no pattern
the result never matches.

`$~` holds the MatchData of the last match, or nil after a failed match. It is
set by `Regexp#match`, `=~` and `===`, and by the `String` methods above.
`match?` leaves it alone. `$&`, `$1` to `$9`, `` $` ``, `$'` and `$+` read it.
`$~` is one global variable for the whole program, not one per method call as
in CRuby.

A pattern the engine does not accept raises `RegexpError`. The message names
the reason and the byte offset in the pattern.

## Syntax

| Syntax | Meaning |
|--------|---------|
| `.` | Any character. A newline only with `m` |
| `[...]`, `[^...]` | Class and negated class. Ranges span code points, so `[あ-ん]` works |
| `\d` `\w` `\s` `\h` `\D` `\W` `\S` `\H` | ASCII digit, word, space and hex-digit classes and their negations, in a class too |
| `[[:alpha:]]`, `[[:^alpha:]]` and the other POSIX classes | ASCII, as `\w` is |
| `\xHH` `\uHHHH` `\u{H...}` `\0` `\177` | A character by its code (octal up to `\177`) |
| `\n` `\t` `\r` `\f` `\v` `\e` `\0` | Control characters |
| `\b` `\B` | Word boundary and its negation, on the ASCII word class |
| `\.` and other escaped punctuation | The literal character |
| `*` `+` `?` `{n}` `{n,}` `{n,m}` | Greedy repetition. `n` and `m` go up to 256 |
| `*?` `+?` `??` `{n,m}?` | Lazy repetition |
| `*+` `++` `?+` | Possessive repetition. `{n,m}+` is `(?:...{n,m})+`, as in Ruby |
| `(...)` | Capture group. Up to 15 groups, or what the build sets with `REGEX_MAX_GROUPS` (at most 64, group 0 included) |
| `(?<name>...)` `(?'name'...)` | Named capture group. With a named group in the pattern, plain `(...)` do not capture, as in CRuby |
| `(?:...)` | Group without capture |
| `(?>...)` | Atomic group, taken as `(?:...)` |
| `(?#...)` | Comment |
| `\1` to `\9`, `\k<name>` `\k'name'` `\k<n>` `\k<-n>` | Backreference. A pattern with one runs on a backtracker |
| `\g<name>` `\g<n>` `\g<-n>` `\g<0>` | Subexpression call, recursive ones too |
| `(?i:...)` `(?-i:...)` `(?i)` | Options for the body, or for the rest of the enclosing group, including the alternatives after it (`a(?i)b\|c` is `a(?i:b\|c)`). `i`, `m` and `x` |
| `(?=...)` `(?!...)` | Lookahead and negative lookahead |
| `(?<=...)` `(?<!...)` | Lookbehind and negative lookbehind. The body must have a bounded length; it may be an alternation of different lengths |
| `\|` | Alternation. The earlier alternative wins at the same position |
| `^` `$` `\A` `\z` `\Z` | Start and end of the string |

## Options

| Option | Effect |
|--------|--------|
| `i` | Case folding for ASCII letters only |
| `m` | `.` also matches a newline |
| `x` | Whitespace, and `#` to the end of the line, are not part of the pattern outside a class |

## Differences from CRuby

- `i` folds ASCII letters only. `/é/i` does not match `É`.
- POSIX classes, and `\p{...}`, which takes their names only, are ASCII:
  `[[:alpha:]]` and `\p{Alpha}` do not match `é`.
- Matching takes time linear in the subject, but for a pattern with a
  backreference: that one runs on a backtracker, which can take far longer.
  It raises `RegexpError` after `REGEX_BT_MAX_STEPS` steps (10,000,000 unless
  the build sets it; 1,000,000 in the r2p2 builds), or when its stack passes
  `REGEX_BT_MAX_STACK` entries of 12 bytes (1,048,576; 1,024 on RP2040 and
  4,096 on RP2350).
- A possessive repeat of more than one character, and an atomic group, are
  taken as the greedy repeat and a plain group: where CRuby finds no match
  because they keep what they took, as in `/(?>a*)a/` on `"aa"`, this finds
  one. A possessive repeat of one character is exact. In a pattern with a
  backreference they do not stop the backtracker from trying the other ways
  through their body either, so such a pattern can run until
  `REGEX_BT_MAX_STEPS`: on an RP2040 that holds the task scheduler up for
  seconds.
- A recursive `\g<n>` is followed 8 levels deep, fewer when the copies
  outgrow the program. A search into input nested deeper raises
  `RegexpError` ("subexpression call nested too deep"), except `match?`
  when it found a match.
- `\w`, `\d`, `\s` and so `\b` are ASCII: `é` is not a word character, so
  `/\bé/` matches in `"aé"`.
- A lookbehind body may have any bounded length, as in `(?<=a|bc)` or
  `(?<=a{1,3})`; CRuby rejects the second. `(?<=a+)` is an error in both.
  Lookaround nests at most 4 deep.
- `$~` is one global for the whole program, not one per method call.
- `sub` and `gsub` do not expand `` \` `` or `\'`. An unterminated `\k<name`
  in a replacement stays as written; CRuby raises.
- `String#[]=` and `Symbol#match` do not take a Regexp.

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

## Changing the engine

Edit `lib/regex_engine.c` and `lib/regex_engine.h` in this gem. They are the
master copy of the engine. Another project can carry the two files as they
are, because they depend on the C library only. Keep them free of PicoRuby
names for that reason.

The engine calls `regex_malloc`, `regex_realloc` and `regex_free`. The host
defines them: `src/mruby/regexp.c` and `src/mrubyc/regexp.c` do it here.
