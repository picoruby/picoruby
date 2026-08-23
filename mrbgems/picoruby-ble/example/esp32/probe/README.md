# picoruby-ble on-device probes

These are not picotest suites. Each file is a standalone program that runs on a
board, drives one part of picoruby-ble against a real peer, and prints one line
of evidence per path it reaches. You read the serial log to judge the result.

They live here rather than under `test/` because `test/` across the tree means
a picotest suite the build can run on its own, and none of these can be: they
need radio, a second device, and a human reading the output.

| Probe | Role | Peer it needs | PASS when |
|---|---|---|---|
| `peripheral_paths_probe.rb` | Peripheral | A Central that connects, subscribes, writes, then disconnects twice | All of P1–P7 appear |
| `central_paths_probe.rb` | Central | A Peripheral advertising as `PBLE-DARWIN` with a notify and a write characteristic | C1, C2 and C3 appear |
| `dynamic_read_probe.rb` | Peripheral | A Central reading the characteristic repeatedly | Every read returns the value pushed for that tick |
| `write_flood_probe.rb` | Peripheral | A Central that writes a known number of frames back to back | The arrival count matches what the peer sent |
| `adv_report_padding_probe.rb` | Central | None; ambient advertising traffic is the input | `raised=yes`, `dlen0 + dlen1 > 0`, `badpad=0`, `fail=0` |

## Running

Any deploy path that puts the file at `/home/app.mrb` works. Capture the serial
output for the length of the run, then read it against the table above.

`peripheral_paths_probe.rb`, `central_paths_probe.rb`, `dynamic_read_probe.rb`
and `write_flood_probe.rb` each need their half of the pair started on the peer
before the board finishes booting, or the first paths go unwitnessed.
`adv_report_padding_probe.rb` needs no peer and no setup.

## `adv_report_padding_probe.rb`

This one checks a contract rather than a round trip, so it is worth spelling
out. `mrblib/ble_advertising_report.rb` refuses a packet shorter than 14 bytes,
a rule inherited from BTstack's `GAP_EVENT_ADVERTISING_REPORT` wire format. A
port that assembles the packet itself has to honour that even when the
advertisement carries no data at all.

`packet_callback` receives the raw packet before `AdvertisingReport` parses it,
which is what makes the port's output checkable from Ruby:

| Byte | Meaning | Expected |
|---|---|---|
| `getbyte(11)` | declared `data_length` | 0 or 1 for the case under test |
| `bytesize` | total length after padding | `>= 14` |
| `getbyte(1)` | length field | `bytesize - 2` |

The probe scans actively, so every nearby device that advertises without
scan-response data answers the `SCAN_REQ` with an empty `SCAN_RSP` and supplies
the `data_length == 0` input. In a normal RF environment these are the majority
of all reports.

It also carries its own negative control: at startup it rebuilds the unpadded
12-byte packet and feeds it to the same parser. If that does not raise, the
probe cannot detect the bug it exists for and the run is invalid.
