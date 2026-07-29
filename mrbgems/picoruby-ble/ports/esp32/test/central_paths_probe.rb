require 'ble'

# Drives and witnesses the central-side paths of the ESP32 NimBLE port.
# Counterpart: ports/darwin/test/e2e_peripheral_test.rb, run under
# PicoRubyBLE.app and advertising as PBLE-DARWIN.
#
#   C1 BLE_write_value_of_characteristic_without_response
#   C2 BLE_write_characteristic_descriptor_using_descriptor_handle
#   C3 BLE_GAP_EVENT_NOTIFY_RX -> 0xA7
#
# C3 is observed by overriding packet_callback: ble_central.rb's
# GATT_EVENT_NOTIFICATION branch is an empty TODO, so the raw packet is the
# only place the port's output is visible. ports/esp32/ble.c:486-497 lays the
# packet out as [0]=0xA7/0xA8, [1]=6+vlen, [2..3]=conn_handle,
# [4..5]=attr_handle, [6..7]=vlen, [8..]=value.
class CentralPathsProbe < BLE
  TARGET_NAME = "PBLE-DARWIN"
  SERVICE     = 0x181A
  CHAR_NOTIFY = 0x2A6E
  CHAR_WRITE  = 0x2A9F

  def initialize
    super(:central)
    @wrote = false
    @subscribed = false
    @notify_seen = 0
  end

  def advertising_report_callback(adv_report)
    return unless adv_report.name_include?(TARGET_NAME)
    puts "[probe] found #{TARGET_NAME} rssi=#{adv_report.rssi}, connecting"
    connect(adv_report)
  end

  def packet_callback(event_packet)
    if event_packet.getbyte(0) == GATT_EVENT_NOTIFICATION ||
       event_packet.getbyte(0) == GATT_EVENT_INDICATION
      @notify_seen += 1
      kind = (event_packet.getbyte(0) == GATT_EVENT_NOTIFICATION) ? "NOTIFICATION" : "INDICATION"
      puts "[probe] C3 raw #{kind} raw=#{event_packet.bytesize} " \
           "handle=#{Utils.little_endian_to_int16(event_packet.byteslice(4, 2))} count=#{@notify_seen}"
    end
    super
  end

  # Array#find / Enumerable#detect are not in the base picoruby VM, so these
  # walk with each and keep the last match.
  def pick(list, uuid32)
    found = nil
    list.each { |e| found = e if e[:uuid32] == uuid32 }
    found
  end

  # Called from the scan loop once discovery has settled. ble_central.rb only
  # reaches :TC_IDLE after the last GATT_EVENT_QUERY_COMPLETE, so that state is
  # what makes @conn_handle and @services safe to use.
  def drive
    return if @wrote
    return if state != :TC_IDLE
    svc = pick(services, SERVICE)
    unless svc
      puts "[probe] service 0x#{SERVICE.to_s(16)} not discovered; services=#{services.size}"
      return
    end
    wr = pick(svc[:characteristics], CHAR_WRITE)
    nt = pick(svc[:characteristics], CHAR_NOTIFY)
    unless wr && nt
      puts "[probe] characteristics missing write=#{!wr.nil?} notify=#{!nt.nil?}"
      return
    end
    rc = write_value_of_characteristic_without_response(@conn_handle, wr[:value_handle], "HELLO")
    puts "[probe] C1 write_value_of_characteristic_without_response rc=#{rc}"

    cccd = pick(nt[:descriptors], CLIENT_CHARACTERISTIC_CONFIGURATION)
    if cccd
      rc = write_characteristic_descriptor_using_descriptor_handle(@conn_handle, cccd[:handle], "\x01\x00")
      puts "[probe] C2 write_characteristic_descriptor_using_descriptor_handle rc=#{rc}"
      @subscribed = true
    else
      # Dumping the descriptors here saves a whole device round-trip if the
      # CCCD landed under a different characteristic than expected.
      puts "[probe] CCCD descriptor not discovered on the notify characteristic"
      puts "[probe] descriptors=#{nt[:descriptors].inspect}"
    end
    @wrote = true
  end

  def heartbeat_callback
    drive
    puts "[probe] state=#{state} notify_seen=#{@notify_seen}"
  end
end

probe = CentralPathsProbe.new
# debug: must be passed to scan -- scan assigns @debug from its own keyword
# (ble_central.rb:63), so setting probe.debug beforehand would be overwritten.
probe.scan(timeout_ms: 120_000, stop_state: :no_stop, debug: true)
