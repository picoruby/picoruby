# picoruby-ble on-device probes

Standalone programs that run on a board, drive one part of picoruby-ble
against a real peer, and print one line of evidence per path reached. Read
the serial log to judge the result -- these need radio, a second device, and
a human reading the output, so they are not a picotest suite under `test/`.

| Probe | Role | Peer it needs | PASS when |
|---|---|---|---|
| `peripheral_paths_probe.rb` | Peripheral | A Central that connects, subscribes, writes, then disconnects twice | All of P1-P7 appear |
| `central_paths_probe.rb` | Central | A Peripheral advertising as `PBLE-DARWIN` with a notify and a write characteristic | C1, C2 and C3 appear |
| `dynamic_read_probe.rb` | Peripheral | A Central reading the characteristic repeatedly | Every read returns the value pushed for that tick |
| `write_flood_probe.rb` | Peripheral | A Central that writes a known number of frames back to back | The arrival count matches what the peer sent |
| `adv_report_padding_probe.rb` | Central | None; ambient advertising traffic is the input | `raised=yes`, `dlen0 + dlen1 > 0`, `badpad=0`, `fail=0` |
| `reinit_probe.rb` | Peripheral/Central, alternating | None | `SUMMARY completed=20 cycles without crash` |

## Running

Any deploy path that puts the file at `/home/app.mrb` works. Capture the serial
output for the length of the run, then read it against the table above.

`peripheral_paths_probe.rb`, `central_paths_probe.rb`, `dynamic_read_probe.rb`
and `write_flood_probe.rb` each need their half of the pair started on the peer
before the board finishes booting, or the first paths go unwitnessed.
`adv_report_padding_probe.rb` and `reinit_probe.rb` need no peer and no setup.

## `adv_report_padding_probe.rb`

`mrblib/ble_advertising_report.rb` refuses a packet shorter than 14 bytes, a
rule inherited from BTstack's `GAP_EVENT_ADVERTISING_REPORT` wire format. A
port that assembles the packet itself has to honour that even when the
advertisement carries no data at all. `packet_callback` receives the raw
packet before `AdvertisingReport` parses it, which is what makes the port's
output checkable from Ruby:

| Byte | Meaning | Expected |
|---|---|---|
| `getbyte(11)` | declared `data_length` | 0 or 1 for the case under test |
| `bytesize` | total length after padding | `>= 14` |
| `getbyte(1)` | length field | `bytesize - 2` |

The probe scans actively, so every nearby device that advertises without
scan-response data answers the `SCAN_REQ` with an empty `SCAN_RSP` and
supplies the `data_length == 0` input -- in a normal RF environment these are
the majority of all reports. It also carries a negative control: at startup
it rebuilds the unpadded 12-byte packet and feeds it to the same parser; if
that does not raise, the run is invalid.
