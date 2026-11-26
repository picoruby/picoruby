# picoruby-random

`Random` for PicoRuby and FemtoRuby.
The random bytes come from the source of the platform.

## Usage

```ruby
require 'random'

key = Random.urandom(32)  # 32 random bytes, a binary String
die = rand(1..6)          # an Integer from 1 to 6
idx = rand(10)            # an Integer from 0 to 9
f   = rand                # a Float, 0.0 or more and less than 1.0
```

Use `Random.urandom` for keys, tokens, nonces and ids that other people must not guess.

## The two VMs

On PicoRuby (the mruby VM), this gem adds `mruby-random` and `mruby-random-urandom`.

- `Random` is the class of `mruby-random`.
- `Random.urandom` reads the source of the platform (see [Sources](#sources)).
- `Random.rand`, `Kernel#rand` and `Random.bytes` use a seeded generator (PCG). Do not use them for keys.

On FemtoRuby (mruby/c), this gem defines `Random` itself.

- All methods read the source of the platform.
- There is no seeded generator. `srand` and `Random.new` do not exist.

## API on FemtoRuby

| Method                   | Return value |
|--------------------------|---|
| `Random.urandom(size)`   | A String of `size` random bytes |
| `Random.bytes(size)`     | The same as `Random.urandom(size)` |
| `Random.rand(max = nil)` | A random number, see below |
| `Kernel#rand(max = nil)` | A random number, see below |

The rand methods take the same arguments as `mruby-random`:

| Argument             | Return value |
|----------------------|---|
| None, `nil` or `0`   | A Float, 0.0 or more and less than 1.0 |
| A positive Integer n | An Integer, 0 or more and less than n |
| A negative Integer   | `Kernel#rand` uses its absolute value. `Random.rand` raises `ArgumentError` |
| A Float              | The method truncates it to an Integer first |
| A Range of Integers  | An Integer in the range, or `nil` for an empty range |
| A Range with a Float | A Float in the range, or `nil` for an empty range |

The rand methods do not have a modulo bias.

## Sources

| Port     | Source            | Note |
|----------|-------------------|---|
| `posix`  | `/dev/urandom`    | In PicoRuby.wasm, Emscripten fills it from `crypto.getRandomValues` |
| `rp2040` | `get_rand_32()`   | pico_rand mixes new entropy on each call: the ROSC on RP2040, the TRNG on RP2350 |
| `esp32`  | `esp_random()`    | The output is true random only while Wi-Fi or Bluetooth runs |

The gem takes one byte from each call of `get_rand_32()` or `esp_random()`.

On a board with the mruby VM, this gem gives `mrb_hal_urandom()` to `mruby-random-urandom`.
A POSIX build takes the posix port of `mruby-random-urandom` instead.

## Errors

- `Random.urandom` raises `RuntimeError` when the source cannot give the bytes. It does not change to a weaker source.
- `Random.urandom` raises `ArgumentError` for a negative size.
