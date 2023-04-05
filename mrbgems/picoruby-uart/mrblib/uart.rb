require "gpio"

class UART
  def initialize(
        unit:,
        txd_pin: -1,
        rxd_pin: -1,
        baudrate: 9600,
        data_bits: 8,
        stop_bits: 1,
        parity: PARITY_NONE,
        flow_control: FLOW_CONTROL_NONE,
        rts_pin: -1,
        cts_pin: -1,
        rx_buffer_size: nil)
    @rx_buffer = UART.open_rx_buffer(rx_buffer_size)
    @unit_num = UART.open_connection(unit.to_s, baudrate, txd_pin, rxd_pin)
    setmode(
      baudrate: nil,
      data_bits: data_bits,
      stop_bits: stop_bits,
      parity: parity,
      flow_control: flow_control,
      rts_pin: rts_pin,
      cts_pin: cts_pin
    )
  end

  def setmode(
    baudrate:     nil,
    data_bits:    nil,
    stop_bits:    nil,
    parity:       nil,
    flow_control: nil,
    rts_pin:      nil,
    cts_pin:      nil)
    _set_baudrate(baudrate) if baudrate
    set_flow_control(flow_control || FLOW_CONTROL_NONE, rts_pin || -1, cts_pin || -1)
    set_format(data_bits || 8, stop_bits || 1, parity || PARITY_NONE)
    self
  end

  # private

  def set_flow_control(flow_control, rts_pin, cts_pin)
    if flow_control == FLOW_CONTROL_NONE
      _set_flow_control(false, false)
    elsif flow_control == FLOW_CONTROL_RTS_CTS
      if rts_pin < 0 && cts_pin < 0
        raise ArgumentError.new("UART: RTS and CTS pins must be specified for hardware flow control")
      else
        _set_function(rts_pin) if 0 <= rts_pin
        _set_function(cts_pin) if 0 <= cts_pin
        _set_flow_control(0 <= rts_pin, 0 <= cts_pin)
      end
    else
      raise ArgumentError.new("UART: invalid flow control mode")
    end
  end

  def set_format(data_bits, stop_bits, parity)
    return if [data_bits, stop_bits, parity].all?{|e| e.nil?}
    if [data_bits, stop_bits, parity].any?{|e| e.nil?}
      raise ArgumentError.new("UART: data_bits, stop_bits and parity must be specified together")
      return
    end
    _set_format(data_bits, stop_bits, parity)
  end
end

