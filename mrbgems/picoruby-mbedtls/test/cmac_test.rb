class CmacTest < Picotest::Test
  def test_cmac_update_digest
    cmac = MbedTLS::CMAC.new('thisisasecretkey', 'aes')
    cmac.update('Hello, World!')
    hex = cmac.digest.bytes.map { |b| b.to_s(16).rjust(2, '0') }.join
    assert_equal('65c9b08b7919565f15a57c13e5fddce6', hex)
  end
end
