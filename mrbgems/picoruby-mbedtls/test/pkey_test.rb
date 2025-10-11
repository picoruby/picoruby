class PkeyTest < Picotest::Test
  def test_pkey_rsa_new
    rsa = MbedTLS::PKey::RSA.new(2048)
    assert_equal(true, rsa.to_pem.start_with?("-----BEGIN RSA PRIVATE KEY-----\n"))
    assert_equal(true, rsa.public?)
    assert_equal(true, rsa.private?)
  end

  def test_pkey_rsa_public_key
    rsa = MbedTLS::PKey::RSA.new(2048)
    public_key = rsa.public_key
    # TODO
    # assert_equal(true, public_key.to_pem.start_with?("-----BEGIN PUBLIC KEY-----\n"))
    # assert_equal(true, public_key.public?)
    # assert_equal(false, public_key.private?)
  end
end
