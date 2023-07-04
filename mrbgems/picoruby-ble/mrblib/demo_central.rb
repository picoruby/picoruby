class MyCentral < BLE::Central
  BTSTACK_EVENT_STATE = 0x60
  GAP_EVENT_ADVERTISING_REPORT = 0xda
#  HCI_STATE_WORKING = 

  def initialize(debug: false)
    @debug = debug
    super
    @last_event = 0
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
  end

  def debug_puts(*args)
    puts(*args) if @debug
  end

  def packet_callback(event_type)
    debug_puts "event type: #{sprintf "%02X", event_type}"
    @last_event = event_type
    case event_type
    when BTSTACK_EVENT_STATE
      puts "packet_event_state: #{packet_event_state}"
     # if packet_event_state == HCI_STATE_WORKING
        puts "Central is up and running on: `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        start_scan
     # end
    when GAP_EVENT_ADVERTISING_REPORT
      show_packet
    end
  end
end
