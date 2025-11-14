require 'quectel_cellular'

def build_body(write_key, data)
  '{"writeKey":"' + write_key + '"' +
    ',"d1":' + data[0].to_s +
    ',"d2":' + data[1].to_s +
    ',"d3":' + data[2].to_s +
    ',"d4":' + data[3].to_s + '}'
end

DOMAIN = "ambidata.io"
CHANNEL_ID = "YOUR_CHANNEL_ID"
WRITE_KEY = "YOUR_WRITE_KEY"
PATH = "/api/v2/channels/#{CHANNEL_ID}/data"

uart = UART.new(unit: :RP2040_UART1, txd_pin: 8, rxd_pin: 9, baudrate: 115200)
client = QuectelCellular::HTTPSClient.new(cacert: "ufs:cacert.pem", uart: uart)
client.check_sim_status
client.configure_and_activate_context

unless client.flst("UFS").any?{|f| f.include?("cacert.pem")}
  client.fupl("cacert.pem", "UFS:cacert.pem")
end
puts client.flst("UFS")

data = [23.4, 45.6, 78.9, 12.3] # Example sensor data
client.post(DOMAIN, PATH, build_body(WRITE_KEY, data), {"Content-Type" => "application/json"})
