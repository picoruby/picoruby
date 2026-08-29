# picoruby-ble examples

Each demo is a pair of programs that talk to each other. The top-level demos
have no board-specific code; `rp2040/` has Pico W demos with real sensors.

| Demo | Roles | Hardware |
|---|---|---|
| `peripheral-central/` | Peripheral, Central | None; a sawtooth stands in for the temperature reading |
| `broadcaster-observer/` | Broadcaster, Observer | None; a sawtooth stands in for the temperature reading |
| `rp2040/peripheral/` | Peripheral | Pico W LED, internal temperature ADC |
| `rp2040/broadcaster/` | Broadcaster | Pico W, plus an LCD and a thermocouple on a breadboard |

`probe/` holds on-device diagnostics for picoruby-ble itself. See `probe/README.md`.

## Peers

Every advertised name contains `PicoRuby`, so any broadcaster pairs with any
observer and any peripheral with any central. A phone or a Mac can stand in
for either half.

## Deploying

`app.rb` goes to `/home/app.rb`; other `.rb` files go to `/lib/`.
