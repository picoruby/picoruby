require "base64"

class CipherTest < Picotest::Test
  def test_chiphers
    ciphers = MbedTLS::Cipher.ciphers

    assert(ciphers.include?("AES-128-CBC"))
    assert(ciphers.include?("AES-192-CBC"))
    assert(ciphers.include?("AES-128-CBC"))
    assert(ciphers.include?("AES-192-CBC"))
    assert(ciphers.include?("AES-256-CBC"))
    assert(ciphers.include?("AES-128-GCM"))
    assert(ciphers.include?("AES-192-GCM"))
    assert(ciphers.include?("AES-256-GCM"))
  end

  def test_cipher_key
    cipher = MbedTLS::Cipher.new("AES-256-GCM")
    cipher.encrypt
    cipher.key = "12345678901234567890123456789012"
    assert_equal(32, cipher.key_len)
  end

  def test_cipher_iv
    cipher = MbedTLS::Cipher.new("AES-256-GCM")
    cipher.encrypt
    cipher.key = "12345678901234567890123456789012"
    cipher.iv = "1234567890ab"
    assert_equal(12, cipher.iv_len)
  end

  def test_cipher_encrypt
    cipher = MbedTLS::Cipher.new("AES-256-GCM")
    cipher.encrypt
    cipher.key = "12345678901234567890123456789012"
    cipher.iv = "1234567890ab"

    cipher.update_ad("a_a_d")
    ciphertext = cipher.update("Hello, World!") + cipher.finish
    tag = cipher.write_tag

    encrypted = ciphertext + tag
    encrypted_base64 = Base64.encode64(encrypted)

    assert_equal(12, cipher.iv_len)
    assert_equal( "Fhx37n8YaTsckUbtLxHFMcoVmoyWMvCXM4RzLG0=", encrypted_base64)
  end

  def test_cipher_decrypt
    cipher = MbedTLS::Cipher.new("AES-256-GCM")
    cipher.decrypt
    cipher.key = "12345678901234567890123456789012"
    cipher.iv = "1234567890ab"

    cipher.update_ad("a_a_d")
    ciphertext = Base64.decode64("Fhx37n8YaTsckUbtLxHFMcoVmoyWMvCXM4RzLG0=")
    ciphertext = ciphertext[0, ciphertext.length - 16]
    tag = ciphertext[ciphertext.length, 16]

    plaintext = cipher.update(ciphertext) + cipher.finish
    assert_equal("Hello, World!", plaintext)
  end
end
