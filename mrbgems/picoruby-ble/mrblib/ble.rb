require 'task'

class BLE
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
  ATT_EVENT_CAN_SEND_NOW = 0xB7

  class AttServer
    def initialize
      @connections = []
      init
    end

    #def packet_callback(event)
    #end

    #def read_callback(conn_handle, att_handle, offset, buffer)
    #end

    #def write_callback(conn_handle, att_handle, offset, buffer)
    #end

    def start
      _start
      @task = Task.new(self) do |server|
        while true
          #if (event, data = server._packet_event)
          if event = server.packet_event
            puts "event: #{event}"
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
  def packet_callback(event)
    case event
    when BLE::BTSTACK_EVENT_STATE
      puts "advertising"
      advertise
    when BLE::HCI_EVENT_DISCONNECTION_COMPLETE
      puts "disconnected"
      enable_le_notification
    when BLE::ATT_EVENT_CAN_SEND_NOW
      puts "can send now"
      notify
    end
  end
end
