# picoruby-spi

SPI (Serial Peripheral Interface) communication library for PicoRuby.

## Usage

```ruby
# Initialize SPI
spi = SPI.new(
  unit: :RP2040_SPI0,
  frequency: 1_000_000,
  sck_pin: 2,
  cipo_pin: 4,  # Controller In Peripheral Out (MISO)
  copi_pin: 3,  # Controller Out Peripheral In (MOSI)
  cs_pin: 5,
  mode: 0
)

# On RP2040 the unit can be omitted; it is inferred from the pins
# (CS is a plain GPIO and does not affect unit selection)
spi = SPI.new(sck_pin: 2, copi_pin: 3, cipo_pin: 4, cs_pin: 5)  # inferred as :RP2040_SPI0

# Write data
spi.write(0x01, 0x02, 0x03)

# Read data
data = spi.read(4)  # Read 4 bytes

# Transfer (write and read simultaneously)
result = spi.transfer(0x9F, additional_read_bytes: 3)

# Use with CS control
spi.select do |s|
  s.write(0xAB)
  data = s.read(4)
end
```

## API

### Constants

- `SPI::MSB_FIRST` - Most significant bit first (default)
- `SPI::LSB_FIRST` - Least significant bit first
- `SPI::DATA_BITS` - Default data bits (8)

### Methods

- `SPI.new(unit: nil, frequency: DEFAULT_FREQUENCY, sck_pin:, cipo_pin:, copi_pin:, cs_pin:, mode: 0, first_bit: MSB_FIRST)` - Initialize SPI. On RP2040 `unit:` is optional and inferred from `sck_pin`/`cipo_pin`/`copi_pin` (only the pins you pass are considered; the CS pin is a plain GPIO and is ignored for unit selection). If a given `unit:` disagrees with the pins, or the pins imply different units, or no unit can be determined, an `ArgumentError` is raised. `:BITBANG` is never inferred and must be requested explicitly. On ESP32 `unit:` is still required.
- `write(*data)` - Write data to SPI
- `read(length, repeated_tx_data = 0)` - Read data from SPI
- `transfer(*data, additional_read_bytes: 0)` - Write and read simultaneously
- `select { block }` - Execute block with CS asserted
- `deselect()` - Deassert CS pin

## Notes

- SPI mode: 0-3 (determines clock polarity and phase)
- Data can be Integer, String, or Array of Integers
- On RP2040, `unit:` can be omitted and is inferred from the pins; the CS pin does not participate in unit selection, and `:BITBANG` must be specified explicitly
