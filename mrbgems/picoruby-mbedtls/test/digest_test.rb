class DigestTest < Picotest::Test
  def test_digest_update_finish
    digest = MbedTLS::Digest.new(:sha256)
    digest.update('Hello, World!')
    hex = digest.finish.bytes.map { |b| b.to_s(16).rjust(2, '0') }.join

    assert_equal('dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f', hex)

    digest.free
  end
end
