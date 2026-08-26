require 'ble'

# Short adv reports (data_length 0/1) must be padded to 14 bytes; see README.md.
class AdvProbe < BLE
  GAP_ADV = 0xda

  def initialize
    super(:central) # only :central reaches packet_callback
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

# Negative control: an unpadded packet must raise, or this probe can't detect the bug.
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

# Active scan (not the default) is what makes short reports arrive.
probe = AdvProbe.new
probe.scan(scan_type: :active, stop_state: :no_stop)
