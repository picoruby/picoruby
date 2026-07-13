require "uart"
require "midibase"

class UART
  class MIDI
    include ::MIDIBASE

    def initialize(
          unit:,
          txd_pin: -1,
          rxd_pin: -1,
          baudrate: 31_250, # some deveces require 38_400 baudrate
          rx_buffer_size: nil,
          time_signature: [4, 4],
          max_sysex_bytes: MIDIBASE::DEFAULT_MAX_SYSEX_BYTES)
      @uart = ::UART.new(
        unit: unit,
        txd_pin: txd_pin,
        rxd_pin: rxd_pin,
        baudrate: baudrate,
        data_bits: 8,
        stop_bits: 1,
        parity: ::UART::PARITY_NONE,
        flow_control: ::UART::FLOW_CONTROL_NONE,
        rx_buffer_size: rx_buffer_size
      )
      initialize_midibase(
        time_signature: time_signature,
        max_sysex_bytes: max_sysex_bytes
      )
    end

    private def midi_read_byte
      @uart.getbyte
    end

    private def midi_read_timestamp_us
      @uart.last_read_timestamp_us || Machine.uptime_us
    end

    private def midi_write_byte(byte)
      @uart.putc(byte)
    end
  end
end
