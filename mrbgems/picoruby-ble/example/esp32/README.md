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
  observer/home/app.rb      # scan-only, no connect
```

`central` and `observer` both need a peer to find. Pair them with:

- another device running `peripheral/app.rb` or `broadcaster/home/app.rb`
  from this same directory, or
- the darwin port under `stackchan-picoruby/pc` (Mac), or
- any BLE scanner/GATT-simulator app (e.g. LightBlue) on the Mac side, to
  visually confirm the broadcaster's advertisement or simulate a peripheral
  for the central demo to connect to.

## On the observer demo's role

`observer/home/app.rb` does what the BLE observer role describes — it scans
and never connects — but it initializes the stack as `:central`. `mrblib`
gates `scan` and `advertising_report_callback` on `:central` alone, so
`:observer` raises immediately on `scan`.

That gate is shared code reaching every PicoRuby platform, so relaxing it is
not this port's business; it belongs to a separate `mrblib/` change. The port
needs nothing from it: `ports/esp32/ble.c:482` already puts `BLE_ROLE_OBSERVER`
and `BLE_ROLE_CENTRAL` on the same advertising-report path, so the demo
exercises the identical port code either way.
