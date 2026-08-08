require "gpio"
begin
  require "irq"
rescue LoadError
  # No picoruby-irq in this build (ESP32); the include below then
  # falls back to a stub UART#irq that raises NotImplementedError.
end

class UART
  # Event bits for UART#irq, matching what the RX interrupt signals
  # through the event bridge (UART_signal_rx in src/uart.c). Further
  # conditions -- break, errors -- become further bits, so the API
  # already has room for them.
  RX_RECEIVE = 1

  # The event-delivery protocol (see picoruby-irq's README): the
  # include provides UART#irq, and #event_source_id (defined in the
  # C glue when the build has the bridge) tells IRQ which source this
  # object signals. rescue, not defined?: the ESP32 build carries no
  # picoruby-irq, and mruby/c has no defined? for constants.
  begin
    include IRQ
  rescue NameError
    # No picoruby-irq in this build: give UART#irq the same visible
    # failure every other no-bridge path raises, instead of a
    # NoMethodError that looks like a typo.
    def irq(event_type, **opts, &callback)
      raise NotImplementedError, "this build has no event bridge"
    end
  end

  def initialize(
        unit: nil,
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
    @unit_num = open_connection(unit.to_s, txd_pin, rxd_pin, rx_buffer_size)
    @baudrate = _set_baudrate(baudrate)
    setmode(
      baudrate: nil,
      data_bits: data_bits,
      stop_bits: stop_bits,
      parity: parity,
      flow_control: flow_control,
      rts_pin: rts_pin,
      cts_pin: cts_pin
    )
    @line_ending = "\n"
  end

  attr_reader :baudrate

  def line_ending=(line_ending)
    unless ["\n", "\r", "\r\n"].include?(line_ending)
      raise ArgumentError.new("UART: invalid line ending")
    end
    @line_ending = line_ending
  end

  def puts(str)
    write str
    unless str.end_with?(@line_ending)
      write @line_ending
    end
    nil
  end

  def setmode(
    baudrate:     nil,
    data_bits:    nil,
    stop_bits:    nil,
    parity:       nil,
    flow_control: nil,
    rts_pin:      nil,
    cts_pin:      nil)
    @baudrate = _set_baudrate(baudrate) if baudrate
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

