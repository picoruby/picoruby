class MyCentral < BLE::Central
  HCI_SUBEVENT_LE_CONNECTION_COMPLETE = 0x01
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_LE_META = 0x3E
  GAP_EVENT_ADVERTISING_REPORT = 0xda

  def initialize(debug)
    super(debug)
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
    @state = :TC_IDLE
    # @state can be:
    #   :TC_OFF
    #   :TC_IDLE
    #   :TC_W4_SCAN_RESULT
    #   :TC_W4_CONNECT
    #   :TC_W4_SERVICE_RESULT
    #   :TC_W4_CHARACTERISTIC_RESULT
    #   :TC_W4_ENABLE_NOTIFICATIONS_COMPLETE
    #   :TC_W4_READY
    @found_devices = {}
    @found_devices_count_limit = 10
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
  end

  def packet_callback(event_packet)
    case event_packet[0]&.ord # event type
    when BTSTACK_EVENT_STATE
      debug_puts "event_packet: #{event_packet.inspect}"
      if event_packet[2]&.ord == BLE::HCI_STATE_WORKING
        debug_puts "Central is up and running on: `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        start_scan
        @state = :TC_W4_SCAN_RESULT
      else
        @state = :TC_OFF
      end
    when GAP_EVENT_ADVERTISING_REPORT
      return unless @state == :TC_W4_SCAN_RESULT
      adv_report = BLE::AdvertisingReport.new(event_packet)
      if @found_devices_count_limit <= @found_devices.count
        return
      end
      unless @found_devices[adv_report.address]
        @found_devices[adv_report.address] = adv_report
        debug_puts adv_report.inspect
        debug_puts ""
      end
    when HCI_EVENT_LE_META
      #debug_puts "event_packet: #{event_packet.inspect}"
      case event_packet[2]&.ord
      when HCI_SUBEVENT_LE_CONNECTION_COMPLETE
        return unless @state == :TC_W4_CONNECT
        conn_handle = BLE::Utils.little_endian_to_int16(packet[4, 2])
        debug_puts "Search for env sensing service."
        @state = :TC_W4_SERVICE_RESULT
      end
    end
  end
end