require 'task'

class BLE
  CYW43_WL_GPIO_LED_PIN = 0

  def self.bd_addr_to_str(addr)
    addr.bytes.map{|b| sprintf("%02X", b)}.join(":")
  end

  class AttServer
    def initialize
      @connections = []
      init
    end

    def start
      hci_power_on
      @task = Task.new(self) do |server|
        while true
          if server.heartbeat_on?
            server.heartbeat_callback
            server.heartbeat_off
          end
          if event = server.packet_event
            server.packet_callback(event)
            server.down_packet_flag
          end
          #if (conn_handle, att_handle, offset, buffer = server._read_event)
          #  server.read_callback(conn_handle, att_handle, offset, buffer)
          #end
          #if (conn_handle, att_handle, offset, buffer = server._write_event)
          #  server.write_callback(conn_handle, att_handle, offset, buffer)
          #end
          sleep_ms 50
        end
      end
      return 0
    end

    def stop
      @task.suspend
      return 0
    end
  end
end

class MyServer < BLE::AttServer
  CYW43_WL_GPIO_LED_PIN = BLE::CYW43_WL_GPIO_LED_PIN
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
  ATT_EVENT_CAN_SEND_NOW = 0xB7
  ATT_EVENT_MTU_EXCHANGE_COMPLETE = 0xB5

  def initialize
    super
    @last_event = 0
    @led_on = false
  end

  def heartbeat_callback
    @counter ||= 0
    @counter += 1
    if @counter == 10
      poll_temp
      if le_notification_enabled?
        puts "le_notification_enabled"
        request_can_send_now_event
      end
      @counter = 0
    end
    case @last_event
    when BTSTACK_EVENT_STATE, HCI_EVENT_DISCONNECTION_COMPLETE
      @led_on = !@led_on
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, @led_on);
    end
  end

  def packet_callback(event)
    puts "packet_callback: #{sprintf "%02X", event}"
    @last_event = event
    case event
    when BTSTACK_EVENT_STATE
      puts "advertising"
      puts "AttServer is up and running on: `#{BLE.bd_addr_to_str(gap_local_bd_addr)}`"
      advertise
    when HCI_EVENT_DISCONNECTION_COMPLETE
      puts "disconnected"
      enable_le_notification
    when ATT_EVENT_MTU_EXCHANGE_COMPLETE
      puts "mtu exchange complete"
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    when ATT_EVENT_CAN_SEND_NOW
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
      puts "can send now. notify"
      notify
      sleep_ms 100
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    end
  end
end
