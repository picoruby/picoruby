class TestBase64 < Picotest::Test
  def setup
    # require the library to be tested using its absolute path
    require "base64"
  end

  def test_encode
    assert_equal "cGljb3J1Ynk=", Base64.encode64("picoruby")
  end

  def test_decode
    assert_equal "picoruby", Base64.decode64("cGljb3J1Ynk=")
  end

  def test_long_string
    long_string = "This is a long string for testing base64 encoding and decoding with PicoRuby."
    encoded = Base64.encode64(long_string)
    decoded = Base64.decode64(encoded)
    assert_equal long_string, decoded
  end
end
