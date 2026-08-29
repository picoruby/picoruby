# picoruby-ble examples

Each demo is a pair of programs that talk to each other, so every one of them
needs two peers. The demos at the top level contain no board-specific code and
run on any port; `rp2040/` holds Pico W versions of two of them that read real
sensors.

| Demo | Roles | Hardware |
|---|---|---|
| `peripheral-central/` | Peripheral, Central | None; a sawtooth stands in for the temperature reading |
| `broadcaster-observer/` | Broadcaster, Observer | None; a sawtooth stands in for the temperature reading |
| `rp2040/peripheral/` | Peripheral | Pico W internal temperature ADC, CYW43 LED |
| `rp2040/broadcaster/` | Broadcaster | Pico W, plus an LCD and a thermocouple on a breadboard |

`probe/` holds on-device diagnostics for picoruby-ble itself: each program
exercises one path of the gem and prints evidence lines to check against the
table in `probe/README.md`.

## Peers

Any broadcaster pairs with any observer, and any peripheral with any central,
across the two layers: the advertised name always contains `PicoRuby`, which
is what the central and the observer look for. A phone or a Mac can also
stand in for either half: any scanner shows the broadcaster's advertisement,
and a GATT simulator can stand in for the peripheral.

## Deploying

`app.rb` goes to `/home/app.rb`. Any other `.rb` file next to it goes
to `/lib/`.
