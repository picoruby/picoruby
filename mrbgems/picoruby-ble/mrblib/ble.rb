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

  class AdvertisingData
    def initialize
      @data = ""
    end

    attr_reader :data

    def add(type, *data)
      length_pos = @data.length
      @data << 0.chr # dummy length
      @data << type.chr
      length = 1
      data.each do |d|
        case d
        when String
          @data << d
          length += d.length
        when Integer
          while 0 < d
            @data << (d & 0xff).chr
            d >>= 8
            length += 1
          end
        else
          raise ArgumentError, "invalid data type: `#{d}`"
        end
      end
      @data[length_pos] = length.chr
    end

    # private_constant :Body
    def self.build(&block)
      b = self.new
      block.call(b)
      adv_data = b.data
      if 31 < adv_data.length
        raise ArgumentError, "too long AdvData. It must be less than 32 bytes."
      end
      adv_data
    end
  end
end


class MyServer < BLE::AttServer
  # for advertising
  APP_AD_FLAGS = 0x06
  BLUETOOTH_DATA_TYPE_FLAGS = 0x01
  BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS = 0x03
  BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09
  # for GATT
  CYW43_WL_GPIO_LED_PIN = BLE::CYW43_WL_GPIO_LED_PIN
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
  ATT_EVENT_CAN_SEND_NOW = 0xB7
  ATT_EVENT_MTU_EXCHANGE_COMPLETE = 0xB5

  def initialize
    super
    @last_event = 0
    @led_on = false
    @counter = 0
    @adv_data = BLE::AdvertisingData.build do |a|
      a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, "PicoRuby BLE")
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x181a)
    end
  end

  def heartbeat_callback
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
    #puts "packet_callback: #{sprintf "%02X", event}"
    @last_event = event
    case event
    when BTSTACK_EVENT_STATE
      puts "AttServer is up and running on: `#{BLE.bd_addr_to_str(gap_local_bd_addr)}`"
      advertise(@adv_data)
    when HCI_EVENT_DISCONNECTION_COMPLETE
      puts "disconnected"
      enable_le_notification
    when ATT_EVENT_MTU_EXCHANGE_COMPLETE
      puts "mtu exchange complete"
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    when ATT_EVENT_CAN_SEND_NOW
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
      notify
      sleep_ms 10
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    end
  end
end
