# picoruby-ble on-device probes

On-device programs that exercise one path of picoruby-ble against a real
peer and print one line of evidence per path reached. Read the serial log
against the table below.

| Probe | Role | Peer it needs | PASS when |
|---|---|---|---|
| `peripheral_paths_probe.rb` | Peripheral | A Central that connects, subscribes, writes, then disconnects twice | All of P1-P7 appear |
| `central_paths_probe.rb` | Central | A Peripheral advertising as `PBLE-DARWIN` with a notify and a write characteristic | C1, C2 and C3 appear |
| `dynamic_read_probe.rb` | Peripheral | A Central reading the characteristic repeatedly | Every read returns the value pushed for that tick |
| `write_flood_probe.rb` | Peripheral | A Central that writes a known number of frames back to back | The arrival count matches what the peer sent |
| `adv_report_padding_probe.rb` | Central | None; ambient advertising traffic is the input | `raised=yes`, `dlen0 + dlen1 > 0`, `badpad=0`, `fail=0` |
| `reinit_probe.rb` | Peripheral/Central, alternating | None | `SUMMARY completed=20 cycles without crash` |

## Usage

Deploy the file to `/home/app.mrb` and capture the serial output.

- `peripheral_paths_probe.rb`, `central_paths_probe.rb`,
  `dynamic_read_probe.rb`, `write_flood_probe.rb`: start the peer's half
  before the board finishes booting.
- `adv_report_padding_probe.rb`, `reinit_probe.rb`: no peer needed.

## Notes

- `adv_report_padding_probe.rb` checks that short advertising reports
  (`data_length` 0 or 1) get padded to the 14-byte minimum
  `ble_advertising_report.rb` requires. It scans actively so short reports
  arrive often, and includes a negative control (an unpadded 12-byte packet
  that must raise).
