class PkeyTest < Picotest::Test
  def test_pkey_rsa_new
    public_key_pem = "-----BEGIN PUBLIC KEY-----\n\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAksL75+FUuLtiYJfDJ47b\n\
4xq8izCFY44R1k/k/ULFaTs0sRcEEe9MNrmD3TOc++lgWSbc4zw0ugjkvzUKei9m\n\
OkkzsI1N8g7U9yyAAJM9i4zGXVOfSkaA9Q20O+mkvrpILe8Pxj3fhEE7Ce1ihlsr\n\
HcXbR2YU/7rhHMzvlPizKNo/afpwHQw5kaiHWLcJ/LDOePgmiNle3USgDRmjbUv1\n\
qLfzb/En+lsw+OcsXVZ1uhKxYu0H66YFDLofnV18rQJ5Skx4ZBRW3ynnaj2mInJI\n\
/s2LnmALyi8oV962phTqcwBfkzRfJDoJIITXD/SvhassmYCvNq9JOQnpLamcmHpa\n\
6QIDAQAB\n\
-----END PUBLIC KEY-----\n"
    rsa = MbedTLS::PKey::RSA.new(public_key_pem)
    assert_equal(public_key_pem, rsa.to_pem)

    rsa.free
  end

  def test_pkey_rsa_generate
    rsa = MbedTLS::PKey::RSA.generate(2048)
    assert_equal(true, rsa.to_pem.start_with?("-----BEGIN RSA PRIVATE KEY-----\n"))

    rsa.free
  end

  def test_pkey_rsa_public_key
    rsa = MbedTLS::PKey::RSA.new(2048)
    public_key = rsa.public_key
    assert_equal(true, public_key.to_pem.start_with?("-----BEGIN PUBLIC KEY-----\n"))

    public_key.free
    rsa.free
  end

  def test_pkey_rsa_to_s
    rsa = MbedTLS::PKey::RSA.new(2048)
    assert_equal(true, rsa.to_s.start_with?("-----BEGIN RSA PRIVATE KEY-----\n"))

    rsa.free
  end

  def test_pkey_rsa_export
    rsa = MbedTLS::PKey::RSA.new(2048)
    assert_equal(true, rsa.to_s.start_with?("-----BEGIN RSA PRIVATE KEY-----\n"))

    rsa.free
  end

  def test_pkey_rsa_public?
    rsa = MbedTLS::PKey::RSA.new(2048)
    assert_equal(true, rsa.public?)

    rsa.free
  end

  def test_pkey_rsa_private?
    rsa = MbedTLS::PKey::RSA.new(2048)
    assert_equal(true, rsa.private?)

    public_key = rsa.public_key
    assert_equal(false, public_key.private?)

    public_key.free
    rsa.free
  end
end
