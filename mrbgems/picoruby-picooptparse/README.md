# picoruby-picooptparse

A minimal, declarative command-line option parser for PicoRuby. CRuby's `optparse` is too featureful for microcontrollers, so `PicoOptionParser` trades flexibility for a small footprint and low allocation.

## Usage

```ruby
require "picooptparse"

parser = PicoOptionParser.new
parser.flag("--help", "-h", help: true, desc: "show help")
parser.flag("--no-color", default: true, desc: "disable color")
parser.on("--mode", "-m", type: :symbol, choices: [:fast, :slow], default: :fast, desc: "run mode")
parser.on("--name", type: :string, default: "anon")
parser.on("--retries", type: :integer, default: 3)

result = parser.parse(ARGV)
if result == :help
  puts parser.usage("myprog")
else
  result[:mode]    #=> :fast
  result[:name]    #=> "anon"
  result[:retries] #=> 3
  result[:color]   #=> true
end
```

## Declaring options

### `on(*names, type:, choices:, default:, desc:, key:)`

A value-taking option.

- `type:` — `:string` (default), `:integer`, or `:symbol`. Controls coercion.
- `choices:` — an Array compared against the *coerced* value; `nil` means unrestricted.
- `default:` — value used when the option is absent (default `nil`).
- `desc:` — help text for `usage`.
- `key:` — override the derived result key.

### `flag(*names, default:, desc:, help:, key:)`

A boolean flag that takes no value.

- A name beginning with `--no-` negates: it strips `no-` for key derivation and sets the key to `false` when seen. A positive boolean name sets `true`.
- `help: true` makes `parse` short-circuit and return `:help` when the flag is seen.

## Result keys

The result key is derived from the first `--long` name: leading dashes and a leading `no-` are stripped, `-` becomes `_`, and the result is a Symbol.
`--midi-out` → `:midi_out`, `--no-color` → `:color`. Access values with `result[:key]`, `result.fetch(:key)`, or `result.to_h`.

## Supported syntax

- `--opt value` (space-separated)
- `--opt=value` (inline)
- short aliases `-x` (with a space-separated value where applicable)

**Not** supported: short-option bundling (`-abc`) or attached short values (`-xval`).

## Errors

`parse` raises `ArgumentError` for:

- unknown option — `unknown option: --frob`
- missing value — `--name requires a value`
- bad integer — `--retries requires an Integer`
- invalid choice — `invalid --mode: bogus`
- value passed to a boolean flag — `--no-color takes no value`
