WIFI_STATUS_STA_STOP                      = 0
WIFI_STATUS_STA_DISCONNECTED              = 1
WIFI_STATUS_STA_SHOULD_START              = 2
WIFI_STATUS_STA_STARTING                  = 3
WIFI_STATUS_STA_START                     = 4
WIFI_STATUS_STA_GOT_IP                    = 5
WIFI_STATUS_STA_GOT_IP_SSID_PW_SAVED      = 6
WIFI_STATUS_STA_GOT_IP_SOMETHING_GOES_BAD = 7
WIFI_STATUS_STA_GOT_IP_SUCCESS_SHOOTING   = 8

HOST = "172.20.10.2"
PORT = 4567.to_s
PATH = "/data"

class Wifi
  def initialize
    start_wifi("hasumi-iPad", "b6kttbfqvee10")
  end

  def make_request(method, path, content)
    method + " " + path + " HTTP/1.1\r\n" +
    "Host: " + HOST + "\r\n" +
    "User-Agent: ESP32\r\n" +
    "accept: application/json\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + content.length.to_s + "\r\n" +
    "\r\n" + content
  end

  def post(data)
    if wifi_status >= WIFI_STATUS_STA_GOT_IP
      request = make_request('POST', PATH, '{"still.image_format":"jpeg"}')
      response = http_post(HOST, PORT, request)
    end
  end
end
