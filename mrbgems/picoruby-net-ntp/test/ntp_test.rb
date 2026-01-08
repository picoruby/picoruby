class NTPTest < Picotest::Test
  def test_ntp_packet_creation
    packet = Net::NTP::Packet.new
    assert_equal(0, packet.leap_indicator)
    assert_equal(3, packet.version)
    assert_equal(3, packet.mode)
  end

  def test_ntp_packet_to_binary
    packet = Net::NTP::Packet.new
    binary = packet.to_binary
    assert_equal(48, binary.length)
  end

  def test_ntp_packet_parse
    # Create a mock NTP response packet
    data = "\x24" + "\x00" * 47  # LI=0, VN=4, Mode=4, rest zeros
    packet = Net::NTP::Packet.new(data)
    assert_equal(0, packet.leap_indicator)
    assert_equal(4, packet.version)
    assert_equal(4, packet.mode)
  end

  def test_ntp_epoch_offset
    # NTP epoch is 1900-01-01, Unix epoch is 1970-01-01
    # Difference should be 2208988800 seconds
    assert_equal(2208988800, Net::NTP::NTP_EPOCH_OFFSET)
  end

  def test_ntp_timestamp_conversion
    packet = Net::NTP::Packet.new
    # Set transmit timestamp to a known value
    # Unix time 0 (1970-01-01 00:00:00) = NTP time 2208988800
    ntp_time = 2208988800 # NTP seconds for Unix epoch
    packet.instance_variable_set(:@transmit_timestamp_sec, ntp_time)
    packet.instance_variable_set(:@transmit_timestamp_frac, 0)
    unix_time = packet.transmit_time
    assert_equal(0, unix_time)
  end

  def test_ntp_packet_binary_packing
    packet = Net::NTP::Packet.new
    binary = packet.to_binary

    # Check first byte (LI=0, VN=3, Mode=3)
    # 0b00_011_011 = 0x1B
    first_byte = binary.bytes[0]
    assert_equal(0x1B, first_byte)

    # Verify packet length
    assert_equal(48, binary.length)
  end

  # Note: The following test requires external network connectivity
  # and is commented out because it depends on external NTP servers
  # Uncomment when running in an environment with network access

  # def test_ntp_get_time
  #   begin
  #     time = Net::NTP.get("pool.ntp.org", 123, 5)
  #     # Should return a Unix timestamp (seconds since 1970)
  #     # Current time should be > 1,700,000,000 (Nov 2023)
  #     assert_true(time > 1_700_000_000)
  #   rescue => e
  #     # Network test may fail in some environments
  #     # Skip this test if network is unavailable
  #   end
  # end
end
