# ESP32 (NimBLE) port examples

Same four demos as `../peripheral-central` and `../broadcaster-observer`,
rewritten for the ESP32 NimBLE port instead of RP2040(W):

- No `CYW43::GPIO` (Pico W's onboard LED is wired through the wifi chip;
  ESP32 has no CYW43) and no `ADC.new(:temperature)` (RP2040-specific
  internal temperature channel — ESP32's `picoruby-adc` port takes a GPIO
  pin number instead, see `ports/esp32/adc.c`).
- No external wiring assumed at all (no LCD/thermo/UART breadboard). Sensor
  values are a synthetic sawtooth so a peer can confirm the payload actually
  changes; all status goes to `puts`/the console instead of an LCD or LED.
- Verified against the current `mrblib/` API (the RP2040 `central/app.rb`
  predates a `mrblib/ble_central.rb` refactor and calls `scan("PicoRuby")` /
  `found_devices`, neither of which exist anymore — this version uses the
  current `scan(...)` + `advertising_report_callback` + `connect(adv_report)`
  flow instead).

## Layout

```
peripheral-central/
  peripheral/app.rb   # BLE::Peripheral, environmental-sensing GATT demo
  central/app.rb      # BLE::Central, scan -> connect -> discover -> read
broadcaster-observer/
  broadcaster/home/app.rb   # BLE::Broadcaster, non-connectable adv only
  observer/home/app.rb      # BLE::Observer, scan-only, no connect
```

`central` and `observer` both need a peer to find. Pair them with:

- another device running `peripheral/app.rb` or `broadcaster/home/app.rb`
  from this same directory, or
- the darwin port under `stackchan-picoruby/pc` (Mac), or
- any BLE scanner/GATT-simulator app (e.g. LightBlue) on the Mac side, to
  visually confirm the broadcaster's advertisement or simulate a peripheral
  for the central demo to connect to.

## Prerequisite

`BLE::Observer` requires the `mrblib/ble.rb` / `mrblib/ble_central.rb` fix
that lets `:observer` share the scan/advertising-report path with `:central`
(only `connect` stays central-only). Without it `observer/home/app.rb` raises
immediately on `scan`.
