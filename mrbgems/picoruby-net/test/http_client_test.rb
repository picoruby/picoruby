class HttpClientTest < Picotest::Test
  def test_http_default_port
    client = Net::HTTPClient.new("localhost")

    assert_equal(80, client.send(:port))
  endl

  def test_https_default_port
    client = Net::HTTPSClient.new("localhost")

    assert_equal(443, client.send(:port))
  end

  def test_custom_http_port
    client = Net::HTTPClient.new("localhost", 8888)

    assert_equal(8888, client.send(:port))
  end

  def test_custom_https_port
    client = Net::HTTPSClient.new("localhost", 8443)

    assert_equal(8443, client.send(:port))
  end
end
