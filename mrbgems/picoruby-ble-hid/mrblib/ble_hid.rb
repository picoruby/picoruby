class BLE
  class HID < BLE
    # Standard 16-bit UUIDs
    GAP_APPEARANCE_UUID          = 0x2A01
    DEVICE_INFORMATION_UUID      = 0x180A
    PNP_ID_UUID                  = 0x2A50
    BATTERY_SERVICE_UUID         = 0x180F
    BATTERY_LEVEL_UUID           = 0x2A19
    HID_SERVICE_UUID             = 0x1812
    HID_INFORMATION_UUID         = 0x2A4A
    HID_REPORT_MAP_UUID          = 0x2A4B
    HID_REPORT_UUID              = 0x2A4D
    HID_CONTROL_POINT_UUID       = 0x2A4C
    PROTOCOL_MODE_UUID           = 0x2A4E
    REPORT_REFERENCE_DESCRIPTOR  = 0x2908

    # Report types for Report Reference descriptor
    REPORT_TYPE_INPUT  = 0x01
    REPORT_TYPE_OUTPUT = 0x02

    # Advertising Data Types
    BLUETOOTH_DATA_TYPE_FLAGS = 0x01
    BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS = 0x03
    BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09
    BLUETOOTH_DATA_TYPE_APPEARANCE = 0x19
    APP_AD_FLAGS = 0x06

    # Appearance value for Keyboard
    APPEARANCE_KEYBOARD = 0x03C1

    # BTstack event types
    BTSTACK_EVENT_STATE = 0x60
    HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
    ATT_EVENT_MTU_EXCHANGE_COMPLETE = 0xB5
    ATT_EVENT_CAN_SEND_NOW = 0xB7

    # HID Information: bcdHID=1.11, bCountryCode=0, Flags=0x02 (normally connectable)
    HID_INFORMATION_VALUE = "\x11\x01\x00\x02"

    # PnP ID: USB vendor source(0x02), Raspberry Pi(0x2E8A), Product(0x0001), Version(0x0001)
    PNP_ID_VALUE = "\x02\x8A\x2E\x01\x00\x01\x00"

    def initialize(name: "RubyHID", mouse: false, consumer: false)
      @mouse_enabled = mouse
      @consumer_enabled = consumer
      @connected = false
      @advertising_started = false

      # Report data
      @keyboard_report = "\x00\x00\x00\x00\x00\x00\x00\x00"
      @consumer_report = "\x00\x00"
      @mouse_report = "\x00\x00\x00\x00"
      @battery_level = 100
      @led_state = 0

      # Notification state
      @keyboard_notify = false
      @consumer_notify = false
      @mouse_notify = false
      @battery_notify = false

      # Pending report flags
      @keyboard_pending = false
      @consumer_pending = false
      @mouse_pending = false

      report_map = _build_report_map

      # Appearance value (little-endian 0x03C1 = Keyboard)
      appearance_value = "\xC1\x03"

      # Build advertising data
      @adv_data = AdvertisingData.build do |a|
        a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
        a.add(BLUETOOTH_DATA_TYPE_APPEARANCE, APPEARANCE_KEYBOARD)
        a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, name)
        a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS,
              HID_SERVICE_UUID)
      end

      # Build GATT database
      db = GattDatabase.new do |d|
        # GAP Service
        d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
          s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, name)
          s.add_characteristic(READ, GAP_APPEARANCE_UUID, READ, appearance_value)
        end

        # Device Information Service
        d.add_service(GATT_PRIMARY_SERVICE_UUID, DEVICE_INFORMATION_UUID) do |s|
          s.add_characteristic(READ, PNP_ID_UUID, READ, PNP_ID_VALUE)
        end

        # Battery Service
        d.add_service(GATT_PRIMARY_SERVICE_UUID, BATTERY_SERVICE_UUID) do |s|
          s.add_characteristic(
            READ | NOTIFY | DYNAMIC, BATTERY_LEVEL_UUID,
            READ | DYNAMIC, ""
          ) do |c|
            c.add_descriptor(
              READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC,
              CLIENT_CHARACTERISTIC_CONFIGURATION, "\x00\x00"
            )
          end
        end
        @battery_handle = d.handle_table[BATTERY_SERVICE_UUID][BATTERY_LEVEL_UUID][:value_handle]
        @battery_cccd_handle = d.handle_table[BATTERY_SERVICE_UUID][BATTERY_LEVEL_UUID][CLIENT_CHARACTERISTIC_CONFIGURATION]

        # HID Service
        d.add_service(GATT_PRIMARY_SERVICE_UUID, HID_SERVICE_UUID) do |s|
          # Protocol Mode (Report Protocol = 0x01)
          s.add_characteristic(
            READ | WRITE_WITHOUT_RESPONSE, PROTOCOL_MODE_UUID,
            READ | WRITE_WITHOUT_RESPONSE, "\x01"
          )

          # HID Information
          s.add_characteristic(READ, HID_INFORMATION_UUID, READ, HID_INFORMATION_VALUE)

          # Report Map
          s.add_characteristic(READ, HID_REPORT_MAP_UUID, READ, report_map)

          # Keyboard Input Report (Report ID=1, Type=Input)
          s.add_characteristic(
            READ | NOTIFY | DYNAMIC, HID_REPORT_UUID,
            READ | DYNAMIC, ""
          ) do |c|
            c.add_descriptor(
              READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC,
              CLIENT_CHARACTERISTIC_CONFIGURATION, "\x00\x00"
            )
            c.add_descriptor(READ, REPORT_REFERENCE_DESCRIPTOR, "\x01\x01")
          end
          # Save handles before next add_characteristic overwrites handle_table entry
          @keyboard_input_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][:value_handle]
          @keyboard_cccd_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][CLIENT_CHARACTERISTIC_CONFIGURATION]

          # Keyboard Output Report (Report ID=1, Type=Output) — LED state from host
          s.add_characteristic(
            READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC, HID_REPORT_UUID,
            READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC, ""
          ) do |c|
            c.add_descriptor(READ, REPORT_REFERENCE_DESCRIPTOR, "\x01\x02")
          end
          @keyboard_output_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][:value_handle]

          # HID Control Point
          s.add_characteristic(
            WRITE_WITHOUT_RESPONSE, HID_CONTROL_POINT_UUID,
            WRITE_WITHOUT_RESPONSE, ""
          )

          if @consumer_enabled
            # Consumer Input Report (Report ID=2, Type=Input)
            s.add_characteristic(
              READ | NOTIFY | DYNAMIC, HID_REPORT_UUID,
              READ | DYNAMIC, ""
            ) do |c|
              c.add_descriptor(
                READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC,
                CLIENT_CHARACTERISTIC_CONFIGURATION, "\x00\x00"
              )
              c.add_descriptor(READ, REPORT_REFERENCE_DESCRIPTOR, "\x02\x01")
            end
            @consumer_input_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][:value_handle]
            @consumer_cccd_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][CLIENT_CHARACTERISTIC_CONFIGURATION]
          end

          if @mouse_enabled
            # Mouse Input Report (Report ID=3, Type=Input)
            s.add_characteristic(
              READ | NOTIFY | DYNAMIC, HID_REPORT_UUID,
              READ | DYNAMIC, ""
            ) do |c|
              c.add_descriptor(
                READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC,
                CLIENT_CHARACTERISTIC_CONFIGURATION, "\x00\x00"
              )
              c.add_descriptor(READ, REPORT_REFERENCE_DESCRIPTOR, "\x03\x01")
            end
            @mouse_input_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][:value_handle]
            @mouse_cccd_handle = d.handle_table[HID_SERVICE_UUID][HID_REPORT_UUID][CLIENT_CHARACTERISTIC_CONFIGURATION]
          end
        end
      end

      super(:peripheral, db.profile_data)
    end

    # --- USB::HID compatible API ---

    # Send keyboard report with modifier and keycode.
    # Report format: [modifier, reserved, keycode, 0, 0, 0, 0, 0]
    def keyboard_send(modifier, keycode)
      @keyboard_report = (modifier & 0xFF).chr +
                         "\x00" +
                         (keycode & 0xFF).chr +
                         "\x00\x00\x00\x00\x00"
      @keyboard_pending = true
      _request_send
      true
    end

    def keyboard_release
      @keyboard_report = "\x00\x00\x00\x00\x00\x00\x00\x00"
      @keyboard_pending = true
      _request_send
      true
    end

    # Returns LED state bitmask from host (Num Lock, Caps Lock, etc.)
    def keyboard_led_state
      @led_state
    end

    # Send mouse report. x, y, wheel are signed (-127..127).
    def mouse_move(x, y, wheel = 0, buttons = 0)
      return false unless @mouse_enabled
      @mouse_report = (buttons & 0xFF).chr +
                      (x & 0xFF).chr +
                      (y & 0xFF).chr +
                      (wheel & 0xFF).chr
      @mouse_pending = true
      _request_send
      true
    end

    # Send consumer control (media) report. code is a 16-bit usage code.
    def media_send(code)
      return false unless @consumer_enabled
      @consumer_report = (code & 0xFF).chr +
                         ((code >> 8) & 0xFF).chr
      @consumer_pending = true
      _request_send
      true
    end

    def send_key(keycode, modifier = 0)
      keyboard_send(modifier, keycode)
    end

    def connected?
      @connected
    end

    def battery_level=(level)
      @battery_level = level < 0 ? 0 : (level > 100 ? 100 : level)
    end

    # --- BLE lifecycle ---

    # Override BLE#start with 1ms polling for keyboard responsiveness.
    # The block is called on every iteration (not just on heartbeat).
    def start(timeout_ms = nil, &block)
      @user_block = block
      total_timeout_ms = 0
      hci_power_control(HCI_POWER_ON)
      while true
        break if timeout_ms && timeout_ms <= total_timeout_ms
        while (packet = pop_packet)
          packet_callback(packet)
        end
        heartbeat_callback if pop_heartbeat
        @user_block&.call
        sleep_ms(1)
        total_timeout_ms += 1
      end
      return total_timeout_ms
    ensure
      hci_power_control(HCI_POWER_OFF)
      @ensure_proc&.call
    end

    def heartbeat_callback
      blink_led
      _start_advertise unless @advertising_started
      _check_cccd
      _check_output_report
      _push_battery_level
    end

    def packet_callback(event_packet)
      case event_packet[0]&.ord
      when BTSTACK_EVENT_STATE
        return unless event_packet[2]&.ord == HCI_STATE_WORKING
        debug_puts "HID Peripheral up on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        _start_advertise
      when HCI_EVENT_DISCONNECTION_COMPLETE
        @connected = false
        @advertising_started = false
        @keyboard_notify = false
        @consumer_notify = false
        @mouse_notify = false
        @battery_notify = false
        @keyboard_pending = false
        @consumer_pending = false
        @mouse_pending = false
        debug_puts "Disconnected, re-advertising"
        _start_advertise
      when ATT_EVENT_MTU_EXCHANGE_COMPLETE
        @connected = true
        debug_puts "Connected (MTU exchange complete)"
      when ATT_EVENT_CAN_SEND_NOW
        _flush_reports
      end
    end

    private

    def _start_advertise
      return if @advertising_started
      advertise(@adv_data)
      @advertising_started = true
      debug_puts "HID advertising started"
    end

    def _check_cccd
      if (kb_cccd = @keyboard_cccd_handle)
        while (data = pop_write_value(kb_cccd))
          @keyboard_notify = (data == "\x01\x00")
          debug_puts "Keyboard notifications #{@keyboard_notify ? 'enabled' : 'disabled'}"
        end
      end
      if (bat_cccd = @battery_cccd_handle)
        while (data = pop_write_value(bat_cccd))
          @battery_notify = (data == "\x01\x00")
          debug_puts "Battery notifications #{@battery_notify ? 'enabled' : 'disabled'}"
        end
      end
      if @consumer_enabled && (con_cccd = @consumer_cccd_handle)
        while (data = pop_write_value(con_cccd))
          @consumer_notify = (data == "\x01\x00")
          debug_puts "Consumer notifications #{@consumer_notify ? 'enabled' : 'disabled'}"
        end
      end
      if @mouse_enabled && (mouse_cccd = @mouse_cccd_handle)
        while (data = pop_write_value(mouse_cccd))
          @mouse_notify = (data == "\x01\x00")
          debug_puts "Mouse notifications #{@mouse_notify ? 'enabled' : 'disabled'}"
        end
      end
    end

    def _check_output_report
      if (output_handle = @keyboard_output_handle)
        while (data = pop_write_value(output_handle))
          byte = data.getbyte(0)
          @led_state = byte if byte
          debug_puts "LED state: #{sprintf('0x%02X', @led_state)}"
        end
      end
    end

    def _push_battery_level
      if (handle = @battery_handle)
        push_read_value(handle, @battery_level.chr)
      end
    end

    def _request_send
      return unless @connected
      if (@keyboard_pending && @keyboard_notify) ||
         (@consumer_enabled && @consumer_pending && @consumer_notify) ||
         (@mouse_enabled && @mouse_pending && @mouse_notify)
        request_can_send_now_event
      end
    end

    # Send one pending report per CAN_SEND_NOW event, then request again if more pending
    def _flush_reports
      if @keyboard_pending && @keyboard_notify
        if (handle = @keyboard_input_handle)
          push_read_value(handle, @keyboard_report)
          notify(handle)
          @keyboard_pending = false
        end
      elsif @consumer_enabled && @consumer_pending && @consumer_notify
        if (handle = @consumer_input_handle)
          push_read_value(handle, @consumer_report)
          notify(handle)
          @consumer_pending = false
        end
      elsif @mouse_enabled && @mouse_pending && @mouse_notify
        if (handle = @mouse_input_handle)
          push_read_value(handle, @mouse_report)
          notify(handle)
          @mouse_pending = false
        end
      end
      _request_send
    end

    # Build HID Report Descriptor (USB HID format)
    # USB HID Report Descriptor constants.
    # Each constant is a single binary string literal (no STRCAT at runtime).
    #
    # Byte-level reference (USB HID spec §6.2.2):
    #   0x05 nn       = Usage Page (nn)
    #   0x09 nn       = Usage (nn)
    #   0xA1 nn       = Collection (01=Application, 00=Physical)
    #   0xC0          = End Collection
    #   0x85 nn       = Report ID (nn)
    #   0x15 nn       = Logical Minimum (signed 8-bit)
    #   0x25 nn       = Logical Maximum (signed 8-bit)
    #   0x26 lo hi    = Logical Maximum (signed 16-bit, little-endian)
    #   0x19 nn       = Usage Minimum (nn)
    #   0x29 nn       = Usage Maximum (nn)
    #   0x2A lo hi    = Usage Maximum (16-bit)
    #   0x75 nn       = Report Size  (bits per field)
    #   0x95 nn       = Report Count (number of fields)
    #   0x81 nn       = Input  (02=Data/Var/Abs, 01=Const, 00=Data/Array, 06=Data/Var/Rel)
    #   0x91 nn       = Output (same flag encoding as Input)

    # Keyboard Report Descriptor (Report ID 1)
    # Input  report: 8 bytes — [modifier(1), reserved(1), keycodes(6)]
    # Output report: 1 byte  — [LED flags(5 bits) + padding(3 bits)]
    #   05 01    UsagePage(GenericDesktop)  09 06    Usage(Keyboard)
    #   A1 01    Collection(Application)   85 01    ReportID(1)
    #   05 07    UsagePage(KeyCodes)        19 E0    UsageMin(0xE0)
    #   29 E7    UsageMax(0xE7)             15 00    LogicalMin(0)
    #   25 01    LogicalMax(1)              75 01    ReportSize(1)
    #   95 08    ReportCount(8)             81 02    Input(Data/Var/Abs)
    #   95 01    ReportCount(1)             75 08    ReportSize(8)
    #   81 01    Input(Const)               05 08    UsagePage(LEDs)
    #   19 01    UsageMin(1)                29 05    UsageMax(5)
    #   95 05    ReportCount(5)             75 01    ReportSize(1)
    #   91 02    Output(Data/Var/Abs)       95 01    ReportCount(1)
    #   75 03    ReportSize(3)              91 01    Output(Const)
    #   05 07    UsagePage(KeyCodes)        19 00    UsageMin(0)
    #   29 65    UsageMax(0x65)             15 00    LogicalMin(0)
    #   25 65    LogicalMax(0x65)           95 06    ReportCount(6)
    #   75 08    ReportSize(8)              81 00    Input(Data/Array)
    #   C0       EndCollection
    KEYBOARD_REPORT_MAP = "\x05\x01\x09\x06\xA1\x01\x85\x01\x05\x07\x19\xE0\x29\xE7\x15\x00\x25\x01\x75\x01\x95\x08\x81\x02\x95\x01\x75\x08\x81\x01\x05\x08\x19\x01\x29\x05\x95\x05\x75\x01\x91\x02\x95\x01\x75\x03\x91\x01\x05\x07\x19\x00\x29\x65\x15\x00\x25\x65\x95\x06\x75\x08\x81\x00\xC0"

    # Consumer Control Report Descriptor (Report ID 2)
    # Input report: 2 bytes — [usage_lo, usage_hi] (16-bit consumer usage code)
    #   05 0C       UsagePage(Consumer)     09 01       Usage(ConsumerControl)
    #   A1 01       Collection(Application) 85 02       ReportID(2)
    #   15 00       LogicalMin(0)           26 FF 03    LogicalMax(0x03FF)
    #   19 00       UsageMin(0)             2A FF 03    UsageMax(0x03FF)
    #   75 10       ReportSize(16)          95 01       ReportCount(1)
    #   81 00       Input(Data/Array)       C0          EndCollection
    CONSUMER_REPORT_MAP = "\x05\x0C\x09\x01\xA1\x01\x85\x02\x15\x00\x26\xFF\x03\x19\x00\x2A\xFF\x03\x75\x10\x95\x01\x81\x00\xC0"

    # Mouse Report Descriptor (Report ID 3)
    # Input report: 4 bytes — [buttons(3 bits+pad), x(signed 8), y(signed 8), wheel(signed 8)]
    #   05 01    UsagePage(GenericDesktop)  09 02    Usage(Mouse)
    #   A1 01    Collection(Application)   85 03    ReportID(3)
    #   09 01    Usage(Pointer)             A1 00    Collection(Physical)
    #   05 09    UsagePage(Buttons)         19 01    UsageMin(1)
    #   29 03    UsageMax(3)                15 00    LogicalMin(0)
    #   25 01    LogicalMax(1)              95 03    ReportCount(3)
    #   75 01    ReportSize(1)              81 02    Input(Data/Var/Abs)
    #   95 01    ReportCount(1)             75 05    ReportSize(5)
    #   81 01    Input(Const)               05 01    UsagePage(GenericDesktop)
    #   09 30    Usage(X)                   09 31    Usage(Y)
    #   09 38    Usage(Wheel)               15 81    LogicalMin(-127)
    #   25 7F    LogicalMax(127)            75 08    ReportSize(8)
    #   95 03    ReportCount(3)             81 06    Input(Data/Var/Rel)
    #   C0       EndCollection(Physical)    C0       EndCollection(Application)
    MOUSE_REPORT_MAP = "\x05\x01\x09\x02\xA1\x01\x85\x03\x09\x01\xA1\x00\x05\x09\x19\x01\x29\x03\x15\x00\x25\x01\x95\x03\x75\x01\x81\x02\x95\x01\x75\x05\x81\x01\x05\x01\x09\x30\x09\x31\x09\x38\x15\x81\x25\x7F\x75\x08\x95\x03\x81\x06\xC0\xC0"

    # Concatenate only the enabled descriptors (at most 2 String#<< calls)
    def _build_report_map
      map = KEYBOARD_REPORT_MAP.dup
      map << CONSUMER_REPORT_MAP if @consumer_enabled
      map << MOUSE_REPORT_MAP if @mouse_enabled
      map
    end
  end
end
