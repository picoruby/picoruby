class HmacTest < Picotest::Test
  def test_hmac_update_digest
    hmac = MbedTLS::HMAC.new('thisisasecretkey', 'sha256')
    hmac.update('Hello, World!')
    hex = hmac.digest.bytes.map { |b| b.to_s(16).rjust(2, '0') }.join
    assert_equal('4ad891fdfc32ebe14c466c138de6d9ea96129a11a139401f81ef9c0165c43608', hex)
  end

  def test_hmac_update_hex_digest
    hmac = MbedTLS::HMAC.new('thisisasecretkey', 'sha256')
    hmac.update('Hello, World!')
    assert_equal('4ad891fdfc32ebe14c466c138de6d9ea96129a11a139401f81ef9c0165c43608', hmac.hexdigest)
  end
end
