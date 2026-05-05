# picoruby-cyw43

CYW43 WiFi chip driver for PicoRuby (used in Raspberry Pi Pico W).

## Usage

```ruby
require 'cyw43'

# Initialize WiFi
CYW43.init("JP")  # Country code

# Connect to WiFi
CYW43.enable_sta_mode
if CYW43.connect_timeout("MySSID", "MyPassword", CYW43::Auth::WPA2_AES_PSK, 10000)
  puts "Connected!"
  puts "Status: #{CYW43.tcpip_link_status_name}"
else
  puts "Connection failed"
end

# Check connection status
if CYW43.link_connected?
  puts "WiFi is connected"
end

# Control onboard LED
led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
led.write(1)  # Turn on
sleep 1
led.write(0)  # Turn off

# Disconnect
CYW43.disconnect
CYW43.disable_sta_mode
```

### Access Point Mode

```ruby
require "cyw43"

CYW43.init("JP")
CYW43.enable_ap_mode("PICO-TIMER", "12345678")

puts "AP started"
puts "AP active?: #{CYW43.ap_active?}"
puts "AP SSID: #{CYW43.ap_ssid}"
puts "AP IP: #{CYW43.ap_ipv4_address}"
puts "AP stations: #{CYW43.ap_station_count}/#{CYW43.ap_max_stations}"
puts "Connect a client, then open http://#{CYW43.ap_ipv4_address}/"
```

### Experimental AP + HTTP Sample

See:

- `mrbgems/picoruby-cyw43/example/pico_w_ap_http_experimental.rb`

This sample is intentionally experimental.
It is meant to confirm whether a client can reach a tiny PicoRuby `TCPServer` over AP mode with PicoRuby's current built-in AP + DHCP behavior.

## API

### CYW43 Module Methods

- `CYW43.init(country = "XX", force: false)` - Initialize WiFi chip
- `CYW43.initialized?()` - Check if initialized
- `CYW43.enable_sta_mode()` - Enable station mode
- `CYW43.disable_sta_mode()` - Disable station mode
- `CYW43.enable_ap_mode(ssid, password, auth = CYW43::Auth::WPA2_AES_PSK)` - Enable access point mode
- `CYW43.disable_ap_mode()` - Disable access point mode
- `CYW43.connect_timeout(ssid, password, auth, timeout_ms)` - Connect to WiFi with timeout
- `CYW43.disconnect()` - Disconnect from WiFi
- `CYW43.link_connected?(print_status = false)` - Check connection status
- `CYW43.tcpip_link_status()` - Get link status code
- `CYW43.tcpip_link_status_name()` - Get link status name
- `CYW43.dhcp_supplied?()` - Check if DHCP supplied IP
- `CYW43.ap_active?()` - Check whether AP mode is currently enabled
- `CYW43.ap_ssid()` - Get the active AP SSID
- `CYW43.ap_max_stations()` - Get the AP association capacity reported by CYW43
- `CYW43.ap_station_count()` - Get the number of currently associated stations
- `CYW43.ap_ipv4_address()` - Get AP-side IPv4 address
- `CYW43.ap_ipv4_netmask()` - Get AP-side IPv4 netmask
- `CYW43.ap_ipv4_gateway()` - Get AP-side IPv4 gateway

### Authentication Types

- `CYW43::Auth::OPEN` - Open network (no password)
- `CYW43::Auth::WPA_TKIP_PSK` - WPA with TKIP
- `CYW43::Auth::WPA2_AES_PSK` - WPA2 with AES
- `CYW43::Auth::WPA2_MIXED_PSK` - WPA2 mixed mode

### Link Status

- `LINK_DOWN` - Link is down
- `LINK_JOIN` - Joining network
- `LINK_NOIP` - Connected but no IP
- `LINK_UP` - Connected with IP
- `LINK_FAIL` - Connection failed
- `LINK_NONET` - No network found
- `LINK_BADAUTH` - Authentication failed

### CYW43::GPIO

- `CYW43::GPIO.new(pin)` - Create GPIO instance (typically for LED)
- `write(value)` - Write 0 or 1
- `read()` - Read current value
- `high?()` - Check if HIGH
- `low?()` - Check if LOW

## Notes

- Specific to Raspberry Pi Pico W and similar boards
- Country code affects allowed WiFi channels
- Timeout is in milliseconds
- `enable_ap_mode` starts a minimal DHCP-backed AP on the default `192.168.4.0/24` network
- PicoRuby exposes AP-side active/SSID/station-count state and AP-side IPv4 information, but not custom AP IP configuration or DHCP control yet
- AP + HTTP behavior still needs more hardware verification across client devices
- See `mrbgems/picoruby-cyw43/example/pico_w_ap_mode.rb` for the minimal AP sample
- See `mrbgems/picoruby-cyw43/example/pico_w_ap_http_experimental.rb` for the experimental AP + HTTP sample
- TODO: add custom AP IP control helpers before documenting a fully supported AP HTTP workflow
