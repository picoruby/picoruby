require 'uart'
class EC21

  DEFAULT_LOG_SIZE = 10

  def initialize(unit:, txd_pin:, rxd_pin:, baudrate:, log_size: DEFAULT_LOG_SIZE)
    @uart = UART.new(unit: unit, txd_pin: txd_pin, rxd_pin: rxd_pin, baudrate: baudrate)
    @log = []
    @log_size = log_size
    # These make unstable, I don't know why
#    @uart.clear_tx_buffer
#    @uart.clear_rx_buffer
    @uart.line_ending = "\r"
  end

  attr_reader :log

  def addlog(cmd, res)
    @log << { cmd: cmd, res: res }
    @log.shift if @log_size < @log.size
    nil
  end

  def call_and_response(cmd, expected_response, error_response = nil, timeout = 1)
    puts cmd
    @uart.puts cmd
    time = 0
    response = ""
    print "  wait #{timeout} "
    while true
      print "."
      sleep_ms 1000 # Machine.using_delay does not work with Object.sleep
      time += 1
      flagment = @uart.read
      response << flagment if flagment
      if response.include?(expected_response)
        addlog(cmd, response)
        puts "OK"
        return true
      end
      if error_response && response.include?(error_response)
        addlog(cmd, response)
        puts "NG"
        return false
      end
      break if timeout < time
    end
    puts "timeout"
    addlog(cmd, "timeout: " + response)
    return false
  end

  def check_sim_status
    call_and_response('ATE0', 'OK', nil, 5) # echo off
    call_and_response('AT+CPIN?', '+CPIN: READY', nil, 20)
    call_and_response('AT+CIMI', 'OK', nil, 5)
    call_and_response('AT+CGREG?', 'OK', nil, 90)
  end

  class SoracomBeamUDP < EC21
    def configure_and_activate_context
      call_and_response('AT+QICSGP=1,1,"SORACOM.IO","sora","sora",0', 'OK', 'ERROR', 10)
      call_and_response('AT+QIACT=1', 'OK', 'ERROR', 15)
      call_and_response('AT+QIACT?', '+QIACT: 1,1,1,', nil, 10)
    end

    def connect_and_send(data)
      call_and_response('AT+QIOPEN=1,0,"UDP","beam.soracom.io",23080,0,0', 'OK', nil, 150) and
        call_and_response("AT+QISEND=0,#{data.length}", '>', nil, 5) and
        call_and_response(data, 'SEND OK', nil, 5)
    end
 end
end

