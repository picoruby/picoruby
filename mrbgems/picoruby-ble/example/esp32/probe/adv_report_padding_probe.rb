require 'ble'

# Every GAP advertising report handed to Ruby has to satisfy what
# mrblib/ble_advertising_report.rb requires of it:
#
#   getbyte(11) = declared data_length
#   bytesize    = total length, must be >= 14
#   getbyte(1)  = length field, must equal bytesize - 2
#
# packet_callback sees the raw packet before AdvertisingReport parses it, so a
# port that assembles the packet itself can be checked byte by byte from Ruby.
# Reports declaring a data_length of 0 or 1 are the ones worth watching: they
# are the shortest a port has to emit, and one that does not pad them up to 14
# bytes produces a packet the parser rejects. Active scanning makes them
# plentiful, since every nearby device advertising without scan-response data
# answers the SCAN_REQ with an empty SCAN_RSP.
class AdvProbe < BLE
  GAP_ADV = 0xda

  def initialize
    # :central, not :observer: scan and packet_callback are gated on :central,
    # so that is the role which reaches the code under test.
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

# NEGATIVE CONTROL: rebuild the packet an unpadded port emits for data_length
# == 0 (p[1] = 10 + dlen, total 12 + dlen bytes) and feed it to the same
# parser. If this does NOT raise, the probe below has no power to detect the
# bug and its PASS would be meaningless.
prefix = "\xda\x0a\x04\x00"      # evt, len=10, event_type=SCAN_RSP, addr_type
addr   = "\x11\x22\x33\x44\x55\x66"
tail   = "\xc0\x00"              # rssi, data_length = 0
unpadded = prefix + addr + tail  # 12 bytes
begin
  BLE::AdvertisingReport.new(unpadded)
  puts "[probe] NEGCTL bytes=#{unpadded.bytesize} raised=no  <-- probe is BLIND"
rescue => e
  puts "[probe] NEGCTL bytes=#{unpadded.bytesize} raised=yes msg=#{e.message}"
end

# Active scanning is what makes short reports arrive; the default is passive.
probe = AdvProbe.new
probe.scan(scan_type: :active, stop_state: :no_stop)
