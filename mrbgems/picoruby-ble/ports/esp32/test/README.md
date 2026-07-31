# picoruby-ble — ESP32 (NimBLE) on-device tests

## `adv_report_padding_probe.rb`

Verifies the short-advertising-report padding in `ports/esp32/ble.c`'s
`synth_advertising_report()`: NimBLE reports advertisements with
`length_data` of 0 or 1, which produce a 12–13 byte packet, while
`mrblib/ble_advertising_report.rb` requires `bytesize >= 14` (a contract
inherited from BTstack's `GAP_EVENT_ADVERTISING_REPORT` wire format).

The probe scans as `:central` and overrides `packet_callback`, which receives
the raw synthesized packet String *before* `AdvertisingReport` parses it. That
makes the C output directly observable from Ruby:

| byte | meaning | expected |
|---|---|---|
| `getbyte(11)` | declared `data_length` | 0 or 1 for the case under test |
| `bytesize` | post-padding total length | `>= 14` |
| `getbyte(1)` | BTstack length field | `bytesize - 2` |

Short reports arrive continuously without any special setup: the ESP32 port
always scans **actively** (`ble_central.c` sets `passive = (scan_type == 0)`,
and `src/mruby/ble_central.c` passes `1` for both `:passive` and `:active`),
so every nearby device that advertises without scan-response data answers the
SCAN_REQ with an empty SCAN_RSP — `event_type == 0x04`, `length_data == 0`.
In a normal RF environment these are the majority of all reports.

The probe carries its own **negative control**: at startup it reconstructs the
exact 12-byte packet the unfixed port emitted and feeds it to the same parser.
If that does not raise, the probe has no power to detect the bug and the run
is invalid.

### Running

Any deploy path that puts the file at `/home/app.mrb` works. With
`stackchan-picoruby`'s rake tasks:

```bash
bundle exec rake r2p2:wipe_storage
sleep 30   # shorter settle makes the picomodem handshake fail
SRC=path/to/adv_report_padding_probe.rb bundle exec rake r2p2:upload_appmrb
SERIAL_LOG=/tmp/probe.log DURATION=60 bundle exec rake r2p2:reset_and_capture
```

### Reading the result

```
[probe] NEGCTL bytes=12 raised=yes msg=packet length must be 14 or more
[probe] SHORT dlen=0 raw=14 len1=12 et=4 bad=0
[probe] SUMMARY total=485 dlen0=369 dlen1=0 badpad=0 fail=0
```

PASS requires all four: `raised=yes` on the negative control, `dlen0 + dlen1 > 0`,
`badpad=0`, `fail=0`.
