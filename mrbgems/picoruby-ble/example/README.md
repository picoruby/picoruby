# picoruby-ble examples

Each demo is a pair of programs that talk to each other, so every one of them
needs two peers. Demos live under the board they were written for; there is
no file shared between boards.

| Board | Demo | Roles | Hardware |
|---|---|---|---|
| `rp2040/peripheral-central/` | Peripheral, Central | Pico W LED, internal temperature ADC |
| `rp2040/broadcaster-observer/` | Broadcaster, Observer | Pico W, plus an LCD and a thermocouple on a breadboard |
| `esp32/peripheral-central/` | Peripheral, Central | None; a sawtooth stands in for the temperature reading |
| `esp32/broadcaster-observer/` | Broadcaster, Observer | None; a sawtooth stands in for the temperature reading |

`esp32/probe/` is not an example -- it holds on-device verification programs
for picoruby-ble itself. See `esp32/probe/README.md`.

## Peers

Run the two halves on two boards, or pair one half with a BLE app on a phone
or a Mac: any scanner shows the broadcaster's advertisement, and a GATT
simulator can stand in for the peripheral.

## Deploying

`<board>/<demo>/<role>/app.rb` goes to `/home/app.rb`. Where a demo also
ships a `lib/` directory, those files go to `/lib/`.
