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

## nRF52 notes

- Units are `NRF52_SPI0`, `NRF52_SPI1` and `NRF52_SPI2`, mapping to SPIM0,
  SPIM1 and SPIM2. `NRF52_SPIM0` and friends are accepted as aliases.
- **SPIM3 is not offered.** It is the only instance reaching 32 Mbps and
  the only one with hardware chip select, but nRF52840 anomaly 198
  corrupts its transmit data, and the workaround has to lock RAM blocks
  through an undocumented register around every transfer.
- SPIM0/1/2 share hardware with TWIM and TWIS of the same index, so a
  given unit number serves either SPI or I2C, never both. Nothing detects
  the clash; assign different unit numbers.
- Any pin can carry any signal: the peripheral selects pins from its own
  side, so there is no fixed pin table as on RP2040.
- `cipo_pin:` may be omitted for a write-only device. `sck_pin:` and
  `copi_pin:` are required, and there is no board default for either.
- Bit rates are 125 kHz, 250 kHz, 500 kHz, 1, 2, 4 and 8 MHz. A request is
  rounded down to one of those where it can be. 125 kHz is the floor, so a
  slower request -- including the library default of 100 kHz -- runs at
  125 kHz; 8 MHz is the ceiling.
- MISO is pulled down, so a deselected or tri-stated slave reads as zeros
  rather than as noise.
- `unit: :BITBANG` is not available; nRF52 offers hardware SPI only.
- 8 bits per transfer only, and transfers are limited to 65535 bytes.
- Chip select is an ordinary GPIO driven by `SPI#select` / `#deselect`,
  not the hardware CSN.
