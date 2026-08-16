# picoruby-ble examples

Each demo is a pair of programs that talk to each other, so every one of them
needs two peers.

| Demo | Roles | Hardware | What it shows |
|---|---|---|---|
| `peripheral-central/` | Peripheral, Central | Two boards, no wiring | Connect, discover services, read values, subscribe, notify |
| `broadcaster-observer/` | Broadcaster, Observer | Two boards, no wiring | Connectionless advertising of a changing payload |
| `rp2040/broadcaster-observer/` | Broadcaster, Observer | Pico W, plus an LCD and a thermocouple on a breadboard | The same connectionless pattern carrying a real sensor reading |

## Layout

`<demo>/` runs on any board picoruby-ble supports.
`<board>/<demo>/` needs that board's external wiring, and its README carries
the pin assignment.

## Boards without the on-chip peripherals

`peripheral-central/` uses the Pico W LED and the RP2040 internal temperature
channel when they are there, and neither is required. Each program defines a
stand-in at the top of the file when the real device is absent, so the demo
body below it reads the same on every board.

| Device | Pico W, Pico 2 W | Other boards |
|---|---|---|
| `CYW43::GPIO` LED | Blinks once per heartbeat | Prints `led: 1` / `led: 0` |
| `ADC.new(:temperature)` | Reads the on-chip sensor | A sawtooth, so a peer can see the value change |

The no-op `CYW43` itself comes from `ble.rb`, because `picoruby-cyw43` is only
built into Pico W and Pico 2 W images (see `mrbgem.rake`). The examples add
`CYW43::GPIO` on top of it.

## Peers

Run the two halves on two boards, or pair one half with a BLE app on a phone
or a Mac: any scanner shows the broadcaster's advertisement, and a GATT
simulator can stand in for the peripheral.

## Deploying

`<role>/app.rb` goes to `/home/app.rb`. Where a demo also ships a `lib/`
directory, those files go to `/lib/`.
