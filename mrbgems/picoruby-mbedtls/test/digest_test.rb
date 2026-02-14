require 'base64'

class DigestTest < Picotest::Test
  def test_digest_sha256_update_finish
    digest = MbedTLS::Digest.new(:sha256)
    digest.update('Hello, World!')
    hex = digest.finish.bytes.map { |b| b.to_s(16).rjust(2, '0') }.join

    assert_equal('dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f', hex)

    digest.free
  end

  def test_digest_sha1_update_finish
    digest = MbedTLS::Digest.new(:sha1)
    digest.update('Hello, World!')
    hex = digest.finish.bytes.map { |b| b.to_s(16).rjust(2, '0') }.join

    assert_equal('0a0a9f2a6772942557ab5355d76af442f8f65e01', hex)

    digest.free
  end

  def test_digest_sha1_websocket_key
    # Test SHA-1 for WebSocket handshake
    # Example from RFC 6455
    key = "dGhlIHNhbXBsZSBub25jZQ=="
    guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

    digest = MbedTLS::Digest.new(:sha1)
    digest.update(key + guid)
    result = digest.finish

    # Expected Accept key: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
    expected = Base64.decode64("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")
    assert_equal(expected, result)

    digest.free
  end
end
