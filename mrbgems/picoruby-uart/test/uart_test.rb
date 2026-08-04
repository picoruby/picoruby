class UARTTest < Picotest::Test
  def setup
    # Inside setup, not at the top of the file: this file is also loaded
    # by host Ruby to enumerate the tests, where `irq` does not exist.
    require 'irq'
    @uart = UART.new(unit: :PICORB_UART_RP2040_UART0, baudrate: 115200)
    # The RX buffer belongs to the unit, not to this object, so it
    # carries whatever an earlier test left in it.
    @uart.clear_rx_buffer
  end

  def test_initialize
    assert_equal 0, @uart.instance_variable_get(:@unit_num)
    assert_equal 115200, @uart.baudrate
  end

  def test_puts
    assert_equal nil, @uart.puts("Hello, UART!")
  end

  def test_getbyte_returns_nil_when_rx_buffer_is_empty
    assert_nil @uart.getbyte
    assert_nil @uart.last_read_timestamp_us
  end

  def test_ungetbyte
    assert_nil @uart.ungetbyte(0x141)
    assert_equal 0x41, @uart.getbyte
    timestamp_us = @uart.last_read_timestamp_us
    assert timestamp_us.is_a?(Integer)
    assert 0 <= timestamp_us
    assert_nil @uart.getbyte
    assert_equal 0, @uart.rx_overflow_count
  end

  def test_putc
    assert_equal 0x141, @uart.putc(0x141)
    assert_equal "ABC", @uart.putc("ABC")
    assert_equal "あいう", @uart.putc("あいう")
    assert_equal "", @uart.putc("")
  end

  def test_putc_rejects_other_types
    assert_raise(TypeError) { @uart.putc(nil) }
  end

  def test_inject_rx_arrives_as_ordinary_input
    assert_equal 3, @uart.inject_rx("abc")
    assert_equal 3, @uart.bytes_available
    assert_equal "abc", @uart.read
  end

  def test_event_source_id_does_not_collide_with_gpio
    assert @uart.event_source_id.is_a?(Integer)
    assert IRQ::SOURCE::GPIO < @uart.event_source_id
    assert @uart.event_source_id < IRQ::MAX_SOURCES
  end

  # The v2 hypothesis: arriving bytes wake a task parked on the queue.
  def test_incoming_bytes_deliver_a_token
    source = @uart.event_source_id
    q = Task::Queue.new
    IRQ.take(source)
    IRQ.unbind(source)
    IRQ.bind(source, q)
    @uart.inject_rx("hi")
    Task.pass
    assert_equal source, q.pop
    assert_equal 1, IRQ.take(source)
    assert_equal "hi", @uart.read
    IRQ.unbind(source)
  end

  def test_a_burst_of_input_produces_one_token
    source = @uart.event_source_id
    q = Task::Queue.new
    IRQ.take(source)
    IRQ.unbind(source)
    IRQ.bind(source, q)
    @uart.inject_rx("a")
    Task.pass
    @uart.inject_rx("b")
    Task.pass
    @uart.inject_rx("c")
    Task.pass
    # Coalesced: one outstanding token stands for the whole burst.
    assert_equal 1, q.size
    assert_equal source, q.pop
    assert_equal 1, IRQ.take(source)
    assert_equal "abc", @uart.read
    IRQ.unbind(source)
  end

  def test_ungetbyte_holds_one_byte_only
    assert_nil @uart.ungetbyte(0x41)
    assert_raise(IOError) { @uart.ungetbyte(0x42) }
    # The first byte is still the one that comes back.
    assert_equal 0x41, @uart.getbyte
    assert_nil @uart.ungetbyte(0x42)
    assert_equal 0x42, @uart.getbyte
  end

  def test_ungetbyte_counts_towards_bytes_available
    assert_equal 0, @uart.bytes_available
    @uart.ungetbyte(0x41)
    assert_equal 1, @uart.bytes_available
    @uart.getbyte
    assert_equal 0, @uart.bytes_available
  end

  def test_read_returns_the_pushed_back_byte_first
    @uart.ungetbyte(0x41)
    assert_equal "A", @uart.read
    assert_nil @uart.read
  end

  def test_read_does_not_update_the_last_read_timestamp
    @uart.ungetbyte(0x41)
    assert_equal "A", @uart.read
    # Only getbyte updates it, and clear_rx_buffer in setup cleared it.
    assert_nil @uart.last_read_timestamp_us
  end

  def test_gets_returns_a_pushed_back_newline
    @uart.ungetbyte(0x0a)
    assert_equal "\n", @uart.gets
    assert_equal 0, @uart.bytes_available
  end

  def test_gets_is_nil_while_no_newline_has_been_pushed_back
    @uart.ungetbyte(0x41)
    assert_nil @uart.gets
    # Observing must not have consumed it.
    assert_equal 1, @uart.bytes_available
  end

  def test_clear_rx_buffer_drops_the_pushed_back_byte
    @uart.ungetbyte(0x41)
    @uart.clear_rx_buffer
    assert_equal 0, @uart.bytes_available
    assert_nil @uart.getbyte
  end

  def test_clear_rx_buffer_forgets_the_last_read_timestamp
    @uart.ungetbyte(0x41)
    assert_equal 0x41, @uart.getbyte
    assert @uart.last_read_timestamp_us.is_a?(Integer)
    @uart.clear_rx_buffer
    assert_nil @uart.last_read_timestamp_us
  end

  def test_reopening_a_unit_keeps_what_it_has_buffered
    @uart.ungetbyte(0x41)
    other = UART.new(unit: :PICORB_UART_RP2040_UART0, baudrate: 9600)
    assert_equal 1, other.bytes_available
    assert_equal 0x41, other.getbyte
  end

  def test_two_objects_on_one_unit_read_one_stream
    other = UART.new(unit: :PICORB_UART_RP2040_UART0, baudrate: 115200)
    @uart.ungetbyte(0x42)
    assert_equal 1, other.bytes_available
    assert_equal 0x42, other.getbyte
    # The byte is gone for both of them; there is only one stream.
    assert_equal 0, @uart.bytes_available
    assert_nil @uart.getbyte
  end

  def test_reopening_a_unit_with_another_buffer_size_raises
    assert_raise(ArgumentError) do
      UART.new(unit: :PICORB_UART_RP2040_UART0, rx_buffer_size: 512)
    end
  end

  def test_rx_buffer_size_must_be_a_power_of_two
    assert_raise(IOError) do
      UART.new(unit: :PICORB_UART_RP2040_UART0, rx_buffer_size: 100)
    end
  end
end
