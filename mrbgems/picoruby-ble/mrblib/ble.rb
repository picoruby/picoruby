require 'task'

class BLE
  class AttServer
    def initialize
      @connections = []
      init
    end

    def packet_callback#(event, data)
      down_packet_flag
    end

    #def read_callback(conn_handle, att_handle, offset, buffer)
    #end

    #def write_callback(conn_handle, att_handle, offset, buffer)
    #end

    def start
      _start
      @task = Task.new(self) do |server|
        while true
          #if (event, data = server._packet_event)
          if server.packet_flag?
            server.packet_callback
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
