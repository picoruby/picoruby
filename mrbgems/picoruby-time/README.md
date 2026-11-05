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

## UTC, offsets and zones

`Time.utc`/`Time.gm` build a UTC time, `Time.new` takes a seventh argument
for the UTC offset (`"+09:00"`, `"UTC"`, `"Z"` or seconds), and a Time
converts with `utc`/`gmtime` (in place), `getutc`/`getgm`, `localtime` and
`getlocal`. `utc?`, `utc_offset`, `zone`, `yday`, `dst?` and `to_a` report
the fields; `zone` is `"UTC"`, the platform's abbreviation for a local
time, or nil for a fixed offset. `Time.at` takes a Float or a second
argument in `:millisecond`, `:usec` or `:nsec`. `strftime` is written in
Ruby with CRuby's conversions and flags, so every target formats alike;
`iso8601`/`xmlschema`, `round`, `floor`, `ceil`, `eql?`, `hash`, `day`,
`month`, `nsec` and `subsec` come with it.
