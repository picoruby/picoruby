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
    def _build_report_map
      map = ""
      # Keyboard (Report ID 1): 8 bytes [modifier, reserved, key1..key6]
      map << "\x05\x01"      # Usage Page (Generic Desktop)
      map << "\x09\x06"      # Usage (Keyboard)
      map << "\xA1\x01"      # Collection (Application)
      map << "\x85\x01"      #   Report ID (1)
      map << "\x05\x07"      #   Usage Page (Key Codes)
      map << "\x19\xE0"      #   Usage Minimum (0xE0)
      map << "\x29\xE7"      #   Usage Maximum (0xE7)
      map << "\x15\x00"      #   Logical Minimum (0)
      map << "\x25\x01"      #   Logical Maximum (1)
      map << "\x75\x01"      #   Report Size (1)
      map << "\x95\x08"      #   Report Count (8)
      map << "\x81\x02"      #   Input (Data, Variable, Absolute) — Modifier byte
      map << "\x95\x01"      #   Report Count (1)
      map << "\x75\x08"      #   Report Size (8)
      map << "\x81\x01"      #   Input (Constant) — Reserved byte
      map << "\x05\x08"      #   Usage Page (LEDs)
      map << "\x19\x01"      #   Usage Minimum (Num Lock)
      map << "\x29\x05"      #   Usage Maximum (Kana)
      map << "\x95\x05"      #   Report Count (5)
      map << "\x75\x01"      #   Report Size (1)
      map << "\x91\x02"      #   Output (Data, Variable, Absolute) — LED report
      map << "\x95\x01"      #   Report Count (1)
      map << "\x75\x03"      #   Report Size (3)
      map << "\x91\x01"      #   Output (Constant) — LED padding
      map << "\x05\x07"      #   Usage Page (Key Codes)
      map << "\x19\x00"      #   Usage Minimum (0)
      map << "\x29\x65"      #   Usage Maximum (101)
      map << "\x15\x00"      #   Logical Minimum (0)
      map << "\x25\x65"      #   Logical Maximum (101)
      map << "\x95\x06"      #   Report Count (6)
      map << "\x75\x08"      #   Report Size (8)
      map << "\x81\x00"      #   Input (Data, Array) — Keycodes
      map << "\xC0"           # End Collection

      if @consumer_enabled
        # Consumer Control (Report ID 2): 2 bytes [usage_lo, usage_hi]
        map << "\x05\x0C"    # Usage Page (Consumer)
        map << "\x09\x01"    # Usage (Consumer Control)
        map << "\xA1\x01"    # Collection (Application)
        map << "\x85\x02"    #   Report ID (2)
        map << "\x15\x00"    #   Logical Minimum (0)
        map << "\x26\xFF\x03" #  Logical Maximum (1023)
        map << "\x19\x00"    #   Usage Minimum (0)
        map << "\x2A\xFF\x03" #  Usage Maximum (1023)
        map << "\x75\x10"    #   Report Size (16)
        map << "\x95\x01"    #   Report Count (1)
        map << "\x81\x00"    #   Input (Data, Array, Absolute)
        map << "\xC0"         # End Collection
      end

      if @mouse_enabled
        # Mouse (Report ID 3): 4 bytes [buttons, x, y, wheel]
        map << "\x05\x01"    # Usage Page (Generic Desktop)
        map << "\x09\x02"    # Usage (Mouse)
        map << "\xA1\x01"    # Collection (Application)
        map << "\x85\x03"    #   Report ID (3)
        map << "\x09\x01"    #   Usage (Pointer)
        map << "\xA1\x00"    #   Collection (Physical)
        map << "\x05\x09"    #     Usage Page (Buttons)
        map << "\x19\x01"    #     Usage Minimum (1)
        map << "\x29\x03"    #     Usage Maximum (3)
        map << "\x15\x00"    #     Logical Minimum (0)
        map << "\x25\x01"    #     Logical Maximum (1)
        map << "\x95\x03"    #     Report Count (3)
        map << "\x75\x01"    #     Report Size (1)
        map << "\x81\x02"    #     Input (Data, Variable, Absolute) — Buttons
        map << "\x95\x01"    #     Report Count (1)
        map << "\x75\x05"    #     Report Size (5)
        map << "\x81\x01"    #     Input (Constant) — Button padding
        map << "\x05\x01"    #     Usage Page (Generic Desktop)
        map << "\x09\x30"    #     Usage (X)
        map << "\x09\x31"    #     Usage (Y)
        map << "\x09\x38"    #     Usage (Wheel)
        map << "\x15\x81"    #     Logical Minimum (-127)
        map << "\x25\x7F"    #     Logical Maximum (127)
        map << "\x75\x08"    #     Report Size (8)
        map << "\x95\x03"    #     Report Count (3)
        map << "\x81\x06"    #     Input (Data, Variable, Relative)
        map << "\xC0"         #   End Collection (Physical)
        map << "\xC0"         # End Collection (Application)
      end

      map
    end
  end
end
