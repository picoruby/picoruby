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

`Time.utc`/`Time.gm` build a UTC time and take microseconds as a seventh
argument, `Time.new` takes a seventh argument for the UTC offset
(`"+09:00"`, `"UTC"`, `"Z"` or seconds) or the same as `in:`, and parses
a String (`Time.new("2026-11-05 12:00:00.5 +09:00")`, `Time.new("2026")`)
with CRuby's grammar and errors. A Time
converts with `utc`/`gmtime` (in place), `getutc`/`getgm`, `localtime` and
`getlocal`. `utc?`, `utc_offset`, `zone`, `yday`, `dst?` and `to_a` report
the fields; `zone` is `"UTC"`, the platform's abbreviation for a local
time, or nil for a fixed offset. `Time.at` takes a Float or a second
argument in `:millisecond`, `:usec` or `:nsec`. `strftime` is written in
Ruby with CRuby's conversions and flags, so every target formats alike;
it is the `Time::Strftime` mixin, which a Date or DateTime with the same
field readers (`year` … `to_i`) can include. `iso8601`/`xmlschema`,
`round`, `floor`, `ceil`, `eql?`, `hash`, `day`, `month`, `nsec` and
`subsec` come with it.
