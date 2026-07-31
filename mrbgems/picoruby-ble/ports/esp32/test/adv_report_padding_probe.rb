require 'ble'

# Deterministic probe for commit 36d99a94's "pad short adv reports" fix.
#
# packet_callback receives the raw synthesized packet String that
# ports/esp32/ble.c's synth_advertising_report() produced, before
# AdvertisingReport parses it. That makes the C-side output directly
# observable from Ruby:
#   getbyte(11) = declared data_length (the dlen the fix pads around)
#   bytesize    = post-padding total length (must be >= 14)
#   getbyte(1)  = BTstack length field (must equal bytesize - 2)
#
# The ESP32 port scans actively (ble_central.c: passive = (scan_type == 0),
# and the Ruby binding passes 1 for both :passive and :active), so empty
# SCAN_RSP packets -- length_data == 0 -- arrive continuously from any nearby
# device that advertises without scan-response data. That is the exact input
# that produced a 12-byte packet and raised ArgumentError before the fix.
class AdvProbe < BLE
  GAP_ADV = 0xda

  def initialize
    # :central, not :observer. The port synthesizes advertising reports on the
    # same path for both (ble.c:482), but mrblib gates scan and
    # packet_callback on :central alone, so :central is the role that reaches
    # the port code under test.
    super(:central)
    @total = 0
    @d0 = 0
    @d1 = 0
    @badpad = 0
    @fail = 0
  end

  def packet_callback(event_packet)
    if event_packet.getbyte(0) == GAP_ADV
      raw  = event_packet.bytesize
      len1 = event_packet.getbyte(1)
      dlen = event_packet.getbyte(11)
      et   = event_packet.getbyte(2)
      @total += 1
      if dlen == 0 || dlen == 1
        @d0 += 1 if dlen == 0
        @d1 += 1 if dlen == 1
        bad = 0
        bad = 1 if raw < 14
        bad = 1 if len1 != raw - 2
        @badpad += bad
        puts "[probe] SHORT dlen=#{dlen} raw=#{raw} len1=#{len1} et=#{et} bad=#{bad}"
      end
    end
    begin
      super
    rescue => e
      @fail += 1
      puts "[probe] PARSE_FAIL #{e.message}"
    end
  end

  def advertising_report_callback(adv_report)
    # Parsed without raising; that is the whole assertion.
  end

  def heartbeat_callback
    puts "[probe] SUMMARY total=#{@total} dlen0=#{@d0} dlen1=#{@d1} badpad=#{@badpad} fail=#{@fail}"
  end
end

# NEGATIVE CONTROL: reconstruct the exact packet the pre-36d99a94 C emitted for
# length_data == 0 (p[1] = 10 + dlen, total 12 + dlen bytes, no padding) and
# feed it to the same parser. If this does NOT raise, the probe below has no
# power to detect the bug and its PASS would be meaningless.
prefix = "\xda\x0a\x04\x00"      # evt, len=10, event_type=SCAN_RSP, addr_type
addr   = "\x11\x22\x33\x44\x55\x66"
tail   = "\xc0\x00"              # rssi, data_length = 0
unfixed = prefix + addr + tail   # 12 bytes: what the unfixed port produced
begin
  BLE::AdvertisingReport.new(unfixed)
  puts "[probe] NEGCTL bytes=#{unfixed.bytesize} raised=no  <-- probe is BLIND"
rescue => e
  puts "[probe] NEGCTL bytes=#{unfixed.bytesize} raised=yes msg=#{e.message}"
end

probe = AdvProbe.new
probe.scan(stop_state: :no_stop)
