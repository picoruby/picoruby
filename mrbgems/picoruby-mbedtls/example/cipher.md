
The example below shows how to encrypt plaintext with PicoRuby's `mbedtls` and decrypt ciphertext with CRuby's `openssl`.

## Encryption and decription with PicoRuby's mbedtls

### Encryption with PicoRuby's mbedtls

```ruby
require 'base64'
require 'mbedtls'

key = "12345678901234567890123456789012"
iv = "1234567890ab"
aad = "a_a_d"
plaintext = "Hello, World!"

p MbedTLS::Cipher.ciphers
#=> ["AES-128-CBC", "AES-192-CBC", "AES-256-CBC", "AES-128-GCM", "AES-192-GCM", "AES-256-GCM"]

## Encryption
cipher = MbedTLS::Cipher.new("AES-256-GCM")
cipher.encrypt
cipher.key = key
cipher.iv = iv
cipher.update_ad(aad)
ciphertext = cipher.update(plaintext) + cipher.finish
tag = cipher.write_tag

encrypted = ciphertext + tag
encrypted_base64 = Base64.encode64(encrypted)
p encrypted_base64
#=> "Fhx37n8YaTsckUbtLxHFMcoVmoyWMvCXM4RzLG0="
```

### Decryption with PicoRuby's mbedtls

```ruby
require 'base64'
require 'mbedtls'

encrypted_base64 = "Fhx37n8YaTsckUbtLxHFMcoVmoyWMvCXM4RzLG0="
key = "12345678901234567890123456789012"
iv = "1234567890ab"
aad = "a_a_d"

cipher = MbedTLS::Cipher.new("AES-256-GCM")
cipher.decrypt
cipher.key = key
cipher.iv = iv
cipher.update_ad(aad)
ciphertext = Base64.decode64(encrypted_base64)
ciphertext = ciphertext[0, ciphertext.length - 16]
tag = ciphertext[ciphertext.length, 16]

plaintext = cipher.update(ciphertext) + cipher.finish
p plaintext
#=> "Hello, World!"
```


## Decryption with CRuby's openssl

```ruby
require 'base64'
require 'openssl'

encrypted_base64 = "Fhx37n8YaTsckUbtLxHFMcoVmoyWMvCXM4RzLG0="
key = "12345678901234567890123456789012"
iv = "1234567890ab"
aad = "a_a_d"

encrypted = Base64.decode64(encrypted_base64)
ciphertext = encrypted[0, encrypted.length - 16]
tag = encrypted[encrypted.length - 16, 16]

decipher = OpenSSL::Cipher.new('aes-256-gcm')
decipher.decrypt
decipher.key = key
decipher.iv = iv
decipher.auth_data = aad
decipher.auth_tag = tag

p decipher.update(ciphertext) + decipher.final
#=> "Hello, World!"
```
