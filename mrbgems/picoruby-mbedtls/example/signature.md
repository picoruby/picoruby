
The example below shows how to sign with RSA private key in CRuby and OpenSSL, and verify with RSA public key in PicoRuby and MbedTLS.

## Sign with RSA private key and verify with RSA public key

### Generate RSA key pair in CRuby and OpenSSL

```ruby
# CRuby
require 'openssl'

rsa_key = OpenSSL::PKey::RSA.new(2048)
PRIVATE_KEY_PEM = rsa_key.to_pem
PUBLIC_KEY_PEM = rsa_key.public_key.to_pem
P PRIVATE_KEY_PEm
P PUBLIC_KEY_PEM
```

### Sign with RSA private key in CRuby and OpenSSL

```ruby
# CRuby
require 'openssl'
require 'base64'

data = "Hello World!"
private_key = OpenSSL::PKey::RSA.new(PRIVATE_KEY_PEM)
signature = private_key.sign(OpenSSL::Digest::SHA256.new, data)
signature_base64 = Base64.encode64(signature)
p signature_base64
```

### Verify with RSA public key in PicoRuby and MbedTLS

```ruby
# PicoRuby
require 'mbedtls'
require 'base64'

PUBLIC_KEY_PEM = File.open("public_key.pem") {|f| f.read}
signature_base64 = File.open("signature_base64.txt") {|f| f.read}

public_key = MbedTLS::PKey::RSA.new(PUBLIC_KEY_PEM)
data = "Hello World!"
signature = Base64.decode64(signature_base64)
if public_key.verify(MbedTLS::Digest.new(:sha256), signature, data)
  p "Verified"
else
  p "Not verified"
end
```

## Create key pair in PicoRuby and MbedTLS

```ruby
# PicoRuby
require 'mbedtls'

rsa = MbedTLS::PKey::RSA.new(2048)
p rsa.to_pem
# => "-----BEGIN RSA PRIVATE KEY-----\nMIIEpAIBAAKCAQEAz8z8z8z8z8z8z8
p rsa.public?
# => true
p rsa.private?
# => true
pub_key = rsa.public_key
p pub_key.to_pem
# => "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
p pub_key.public?
# => true
p pub_key.private?
# => false
```
