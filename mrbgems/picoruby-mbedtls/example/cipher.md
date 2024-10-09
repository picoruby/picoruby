
The example below shows how to encrypt plaintext with PicoRuby's `mbedtls` and decrypt ciphertext with CRuby's `openssl`.

```ruby
# PicoRuby
require 'mbedtls'

key = "12345678901234567890123456789012"
iv = "1234567890ab"
aad = "a_a_d"

plaintext = "Hello, World!"

cipher = MbedTLS::Cipher.new(:aes_256_gcm, key, :encrypt)
cipher.set_iv(iv)
cipher.update_ad(aad)
ciphertext = cipher.update(plaintext) + cipher.finish
tag = cipher.write_tag

p ciphertext
#=> "\x16\x1Cw\xEE\x7F\x18i;\x1C\x91F\xED/"
p tag
#=> "\x11\xC51\xCA\x15\x9A\x8C\x962\xF0\x973\x84s,m"
```

```ruby
# CRuby
require 'openssl'

key = "12345678901234567890123456789012"
iv = "1234567890ab"
aad = "a_a_d"

ciphertext = "\x16\x1Cw\xEE\x7F\x18i;\x1C\x91F\xED/"
tag = "\x11\xC51\xCA\x15\x9A\x8C\x962\xF0\x973\x84s,m"

decipher = OpenSSL::Cipher.new('aes-256-gcm')
decipher.decrypt
decipher.key = key
decipher.iv = iv
decipher.auth_data = aad
decipher.auth_tag = tag

p decipher.update(ciphertext) + decipher.final
#=> "Hello, World!"
```
