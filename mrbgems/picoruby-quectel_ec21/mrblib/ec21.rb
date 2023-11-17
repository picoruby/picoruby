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

  def assert_response(cmd, expected_response, timeout_ms = 1000)
    @uart.puts cmd
    time = 0
    while @uart.bytes_available < expected_response.length
      sleep_ms 10
      time += 10
      if timeout_ms < time
        addlog(cmd, 'timeout')
        return false
      end
    end
    res = @uart.read
    addlog(cmd, res)
    return res.include?(expected_response)
  end

  def check_sim_status
    assert_response('AT+CPIN?', '+CPIN: READY')
    assert_response('AT+CIMI', 'OK')
    return false unless assert_response('AT+CGREG?', 'OK')
    # Sometimes 558, invalid parameters error occurs here, but it can be sent
    assert_response('AT+QICSGP=1,1,"SORACOM.IO","sora","sora",0', 'OK')
    true
  end

  class SoracomBeamUDP < EC21
    def connect_and_send(data)
      # It will be an error (error code 563) if it is already activated, but proceed
      assert_response('AT+QIACT=1', 'OK')
      # Ignore error. I don't know why, but it works.
      assert_response('AT+QIACT?', '+QIACT: 1,1,1,')
      return false unless assert_response('AT+QIOPEN=1,0,"UDP","beam.soracom.io",23080,0,0', 'OK')
      return false unless assert_response('AT+QISEND=0,#{data.length}', '>')
      return false unless assert_response(data, 'SEND OK')
      assert_response('AT+QICLOSE=0', 'OK')
      assert_response('AT+QIDEACT=1', 'OK')
      true
    end
 end
end

