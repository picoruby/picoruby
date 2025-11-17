# Time class for PicoRuby

## Timezone Support on RP2040 (newlib)

On RP2040 using newlib, `tzset()` supports only **fixed-offset POSIX-style TZ strings**.
Complex tzdata names or daylight saving rules are **not supported**.

**Format:** STDoffset and STDoffsetDST

- `STD` – standard timezone abbreviation (3+ characters, e.g., `JST`, `EST`, `UTC`)
- `offset` – UTC offset in hours (positive means behind UTC, negative means ahead, following POSIX rules)
- `DST` – optional, ignored by newlib

**Examples:**

- `JST-9` → UTC+9 (Japan Standard Time)
- `EST5` → UTC-5 (Eastern Standard Time)
- `PST8` → UTC-8 (Pacific Standard Time)
- `UTC0` → UTC+0

**Unsupported:**

- Region names: `Asia/Tokyo`, `Europe/London`, `UTC`
- Complex DST rules: `EST5EDT,M3.2.0/2,M11.1.0/2`

**Usage:**

```ruby
ENV["TZ"] = "JST-9"
```
