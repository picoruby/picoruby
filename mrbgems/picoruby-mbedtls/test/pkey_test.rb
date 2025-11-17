require "base64"

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

  def test_pkey_verify
    public_key_pem = "-----BEGIN PUBLIC KEY-----\n\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAly3xGezjd1cTPrmBr9kj\n\
sV/d3pGh+jvc99gDPf/gKdP3eoTlRCDdR0wuJgO2/ywAdBOjOoHWspCbgKtIMAiL\n\
RnXGtz+OneXvwn0eKfO9HxiNqO33umsbzfa5XhRB95WkLS3zlYiTS1lXy9aXrTmO\n\
lLDV6r9hrLLe9V2Or3I0hqklol2jGdMAOe33lQVYvRt7oseLK/cAMLYvV3gzxeeS\n\
U3BxVa/hahmjImR5Q+ABu7AhvHvOZEhng4FeBqWLcdYudSkz8L7KVaQxpGJaEjeM\n\
NUmUglUJ2Qqb3TNMUiEF4P9Ccbh2zJldfvdmbkw2S2cB20z1cEFqhzxmxDhL1ODi\n\
3wIDAQAB\n\
-----END PUBLIC KEY-----"
    signature_base64 = "R3v7uR53JmPjov08xeGMqqXxB+LwGme/NWAJbVsiwgKecR1bkWJWsMrS6tSL\
+VGKiIPth7u6t3ptkJ6dZbPaUGdmdPGnpuMyKH4/sZvhqwPr1bZFQWI730aP\
vlJ2Uy9feSXEF1j9ZKMYUou07EtlZIJ9GgddyaezcyJxuh2zMkOmk9wH4zo9\
1OJ3abVMzay0zHGhfLJkB5Aibz6xXNqablaLgGKzwwtyuVh+grLbY5TjN2hT\
GyNQRyrQEnJI6o8YjdGccKZf/4wb600C/ZIlYYvpudEfY7lBHqgZVpOaHPgF\
6xKGDb9e63zorF/SuoBgzHCoTxNO2hG7T4y7aoga9g=="

    public_key = MbedTLS::PKey::RSA.new(public_key_pem)
    data = 'Hello World!'
    signature = Base64.decode64(signature_base64)
    digest = MbedTLS::Digest.new(:sha256)

    assert_equal(true, public_key.verify(digest, signature, data))
    digest.free
  end

  def test_pkey_sign
    rsa = MbedTLS::PKey::RSA.new(2048)
    data = 'Hello World!'
    digest = MbedTLS::Digest.new(:sha256)

    signature = rsa.sign(digest, data)
    assert_equal(true, rsa.public_key.verify(digest, signature, data))
  end
end
