class MyCentral < BLE::Central
  HCI_SUBEVENT_LE_CONNECTION_COMPLETE = 0x01
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_LE_META = 0x3E
  GAP_EVENT_ADVERTISING_REPORT = 0xda
#  HCI_STATE_WORKING = 

  def initialize(debug)
    super(debug)
    @last_event = 0
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
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
  end

  def packet_callback(event_type)
    debug_puts "event type: #{sprintf "%02X", event_type}" unless @last_event == event_type
    @last_event = event_type
    packet = get_packet #.each_byte { |b| printf("%02X ", b) }
    case event_type
    when BTSTACK_EVENT_STATE
      debug_puts "packet_event_state: #{packet_event_state}, packet[2]: #{packet[2].ord}"
      if packet[2]&.ord == 0 #HCI_STATE_WORKING
        puts "Central is up and running on: `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        start_scan
        @state = :TC_W4_SCAN_RESULT
      else
        @state = :TC_OFF
      end
    when GAP_EVENT_ADVERTISING_REPORT
      return unless @state == :TC_W4_SCAN_RESULT
      debug_puts "Advertising report received."
      packet.each_byte { |b| printf("%02X ", b) }
      puts
    when HCI_EVENT_LE_META
      case packet[2]&.ord
      when HCI_SUBEVENT_LE_CONNECTION_COMPLETE
        return unless @state == :TC_W4_CONNECT
        conn_handle = BLE::Utils.little_endian_to_int16(packet[4,2])
        debug_puts "Search for env sensing service."
        @state = :TC_W4_SERVICE_RESULT
      end
    end
  end
end
