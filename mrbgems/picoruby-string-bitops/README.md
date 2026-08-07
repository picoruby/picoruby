# picoruby-string-bitops

Bit operations for `String` on FemtoRuby (mruby/c).

This is the FemtoRuby port of CRuby [Feature #22118] "Introduce Basic
Bit Operations into String".  On the mruby VM (PicoRuby/MicroRuby),
the same API is provided by the `mruby-string-bitops` mgem bundled in
`picoruby-mruby`; this gem intentionally supports only mruby/c and
fails to compile for `PICORB_VM_MRUBY`.

Strings are treated as byte buffers; all operations are independent of
string encoding.

## Methods

### Single-bit operations

- `String#bit_get(offset, lsb_first: true)` - returns `0` or `1`, or
  `nil` when `offset` is beyond the end of the string
- `String#bit_set?(offset, lsb_first: true)` - returns `true` or
  `false`, or `nil` when `offset` is beyond the end of the string
- `String#bit_set(offset, lsb_first: true)` - sets the bit to 1;
  returns `self`
- `String#bit_clear(offset, lsb_first: true)` - sets the bit to 0;
  returns `self`
- `String#bit_flip(offset, lsb_first: true)` - inverts the bit;
  returns `self`

`offset` is a zero-based bit offset.  By default, bits within each
byte are numbered from least-significant to most-significant.  With
`lsb_first: false`, byte order is unchanged but bits within each byte
are numbered from most-significant to least-significant.

`IndexError` is raised when `offset` is negative, or (for the mutating
methods) when it is beyond the end of the string.

### Whole-string operations

- `String#bit_count` - number of set bits (population count)
- `String#bitwise_not` / `String#bitwise_not!` - bitwise complement
- `String#bitwise_and(other)` / `String#bitwise_and!(other)`
- `String#bitwise_or(other)` / `String#bitwise_or!(other)`
- `String#bitwise_xor(other)` / `String#bitwise_xor!(other)`

The binary operations require both strings to have the same byte
length, otherwise `ArgumentError` is raised.  The non-bang variants
return a new string; the bang variants mutate `self` in place.

## Example

```ruby
require "string-bitops"

s = "\x00\x00"
s.bit_set(3)          # => "\x08\x00"
s.bit_set?(3)         # => true
s.bit_count           # => 1

"\xF0".bitwise_and("\xCC")  # => "\xC0"
"\x0F".bitwise_or("\xF0")   # => "\xFF"
"\xFF".bitwise_not          # => "\x00"
```

## Implementation notes

The bulk kernels (`bit_count` and the `bitwise_*` family) process one
machine word per iteration with 4x unrolling.  The word width follows
the pointer width of the target, so 32-bit targets use 32-bit words
and avoid emulated 64-bit arithmetic.  On GNU-compatible compilers,
word-aligned buffers are accessed directly through a `may_alias` word
pointer, which yields true word loads even on cores without unaligned
access support (e.g. Cortex-M0+); unaligned buffers fall back to a
`memcpy`-based word loop.

## Differences from CRuby

- Offsets must be `Integer` (up to `mrbc_int_t`); `to_int` conversion
  and Bignum offsets are not supported (`TypeError`).
- Binary operands must be real `String`s; `to_str` conversion is not
  performed (`TypeError`).
- mruby/c has no `freeze` or `Encoding`, so the corresponding CRuby
  behaviors (`FrozenError`, BINARY result encoding) do not apply.
- `bit_count` raises `RangeError` when the count does not fit in
  `mrbc_int_t`; this is only reachable under `MRBC_INT16`.

## License

MIT
