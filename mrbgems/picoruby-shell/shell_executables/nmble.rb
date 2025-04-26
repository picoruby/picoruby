require 'ble'
require "mbedtls"
require "base64"
require "yaml"

class WifiConfigPeripheral < BLE
  # BLE Advertising Data
  APP_AD_FLAGS = 0x06
  BLUETOOTH_DATA_TYPE_FLAGS = 0x01
  BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09
  BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS = 0x03

  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
  ATT_EVENT_CAN_SEND_NOW = 0xB7
  ATT_EVENT_MTU_EXCHANGE_COMPLETE = 0xB5
  GATT_CHARACTERISTIC_USER_DESCRIPTION = 0x2901
  # Custom BLE Service UUIDs
  WIFI_CONFIG = 0x181C
  CHARACTERISTIC_SSID = 0x2AAB
  CHARACTERISTIC_PASSWORD = 0x2AAC
  CHARACTERISTIC_COUNTRY = 0x2AAD
  CHARACTERISTIC_WATCHDOG = 0x2AAE

  def initialize(name = "R2P2 BLE")
    # Setup BLE advertising data
    @adv_data = BLE::AdvertisingData.build do |a|
      a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, name)
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, WIFI_CONFIG)
    end

    # Setup BLE GATT Database
    db = BLE::GattDatabase.new do |db|
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, WIFI_CONFIG) do |s|
        w_d = BLE::WRITE|BLE::DYNAMIC
        r = BLE::READ
        s.add_characteristic(w_d, CHARACTERISTIC_SSID, w_d, "") do |c|
          c.add_descriptor(r, GATT_CHARACTERISTIC_USER_DESCRIPTION, "SSID")
        end
        s.add_characteristic(w_d, CHARACTERISTIC_PASSWORD, w_d, "") do |c|
          c.add_descriptor(r, GATT_CHARACTERISTIC_USER_DESCRIPTION, "PSWD")
        end
        s.add_characteristic(w_d, CHARACTERISTIC_COUNTRY, w_d, "") do |c|
          c.add_descriptor(r, GATT_CHARACTERISTIC_USER_DESCRIPTION, "CTRY")
        end
        s.add_characteristic(w_d, CHARACTERISTIC_WATCHDOG, w_d, "") do |c|
          c.add_descriptor(r, GATT_CHARACTERISTIC_USER_DESCRIPTION, "WTDG")
        end
      end
    end

    @ssid_handle = db.handle_table[WIFI_CONFIG][CHARACTERISTIC_SSID][:value_handle]
    @password_handle = db.handle_table[WIFI_CONFIG][CHARACTERISTIC_PASSWORD][:value_handle]
    @country_handle = db.handle_table[WIFI_CONFIG][CHARACTERISTIC_COUNTRY][:value_handle]
    @watchdog_handle = db.handle_table[WIFI_CONFIG][CHARACTERISTIC_WATCHDOG][:value_handle]
    @configuration_handle = db.handle_table[WIFI_CONFIG][CHARACTERISTIC_SSID][BLE::CLIENT_CHARACTERISTIC_CONFIGURATION]

    super(:peripheral, db.profile_data)
    @led_blink = true
  end

  def heartbeat_callback
    blink_led if @led_blink

    if (ssid_value = pop_write_value(@ssid_handle)) && ssid_value != @ssid
      @ssid = ssid_value
      debug_puts "Received SSID: #{ssid_value}"
    end
    if (password_value = pop_write_value(@password_handle)) && password_value != @password
      @password = password_value
      debug_puts "Received Password: ******"
    end
    if (country_value = pop_write_value(@country_handle)) && country_value != @country
      @country = country_value
      debug_puts "Received Country Code: #{country_value}"
    end
    if (watchdog_value = pop_write_value(@watchdog_handle)) && watchdog_value != @watchdog
      @watchdog = watchdog_value
      debug_puts "Received Watchdog: #{watchdog_value}"
    end

    if @ssid && @password && @country && @watchdog
      save_wifi_config
      @ssid = @password = @country = @watchdog = nil
      puts "\nConfiguration saved. Please reboot the device.\n"
    end
  end

  def save_wifi_config
    doc = {
      "country_code" => @country,
      "wifi" => {
        "ssid" => @ssid,
        "encoded_password" => Base64.encode64(encrypt_password(@password)),
        "auto_connect" => true,
        "watchdog" => (@watchdog == 'y')
      }
    }
    File.open(ENV['WIFI_CONFIG_PATH'], "w") do |f|
      f.write YAML.dump(doc)
    end
  end

  def encrypt_password(password)
    cipher = MbedTLS::Cipher.new("AES-256-CBC")
    cipher.encrypt
    key_len = cipher.key_len
    iv_len = cipher.iv_len
    unique_id = Machine.unique_id
    len = unique_id.length
    cipher.key = (unique_id * ((key_len / len + 1) * len))[0, key_len].to_s
    cipher.iv = (unique_id * ((iv_len / len + 1) * len))[0, iv_len].to_s
    ciphertext = cipher.update(password) + cipher.finish
    tag = cipher.write_tag
    tag + ciphertext
  end

  def packet_callback(event_packet)
    puts "packet_callback"
    case event_packet[0]&.ord # event type
    when BTSTACK_EVENT_STATE
      return unless event_packet[2]&.ord == BLE::HCI_STATE_WORKING
      debug_puts "Peripheral is running on: `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
      advertise(@adv_data)
    when HCI_EVENT_DISCONNECTION_COMPLETE
      @led_blink = true
      debug_puts "Device disconnected"
    when ATT_EVENT_MTU_EXCHANGE_COMPLETE
      debug_puts "MTU exchange complete"
      @led_blink = false
      @led&.write(1)
    when ATT_EVENT_CAN_SEND_NOW
      debug_puts "Ready to send data"
    else
      debug_puts "Unhandled event: #{event_packet.inspect}"
    end
  end
end

puts "\nLaunching BLE peripheral to configure WiFi connection."

# Start BLE Peripheral
require 'rng'
name = "R2P2 BLE #{RNG.random_int.abs.to_s[0,4]}"
peri = WifiConfigPeripheral.new(name)
peri.debug = true
puts "\nOpen https://picoruby.github.io/wifi"
puts "and find the device with name: '#{name}'\n"
peri.start

