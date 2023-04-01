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
    @unit_num = UART._init(unit, baudrate, txd_pin, rxd_pin, rx_buffer_size)
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
    UART._set_baudrate(@unit_num, baudrate) if baudrate
    set_flow_control(flow_control || FLOW_CONTROL_NONE, rts_pin || -1, cts_pin || -1)
    set_format(data_bits || 8, stop_bits || 1, parity || PARITY_NONE)
    self
  end

  # private

  def set_flow_control(flow_control, rts_pin, cts_pin)
    if flow_control == FLOW_CONTROL_NONE
      UART._set_flow_control(@unit_num, false, false)
    elsif flow_control == FLOW_CONTROL_RTS_CTS
      if rts_pin < 0 && cts_pin < 0
        raise ArgumentError.new("UART: RTS and/or CTS pins must be specified for hardware flow control")
      else
        UART._set_function(rts_pin) if 0 <= rts_pin
        UART._set_function(cts_pin) if 0 <= cts_pin
        UART._set_flow_control(@unit_num, 0 <= rts_pin, 0 <= cts_pin)
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
    UART._set_format(@unit_num, data_bits, stop_bits, parity)
  end
end

