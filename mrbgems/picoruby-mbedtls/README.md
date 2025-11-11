# picoruby-mbedtls

Mbed TLS cryptographic library for PicoRuby - encryption, hashing, and crypto operations.

## Overview

Provides cryptographic functions using Mbed TLS library including hash functions, HMAC, CMAC, cipher operations, and public key cryptography.

## Usage

### Message Digest (Hashing)

```ruby
require 'mbedtls'

# SHA-256 hash
digest = MbedTLS::Digest.new(:sha256)
digest.update("Hello, ")
digest.update("World!")
hash = digest.finish
puts hash.unpack("H*").first  # Hex representation
digest.free
```

### HMAC (Hash-based Message Authentication Code)

```ruby
# HMAC-SHA256
hmac = MbedTLS::HMAC.new(:sha256, "secret_key")
hmac.update("message")
mac = hmac.finish
hmac.free
```

### CMAC (Cipher-based MAC)

```ruby
# CMAC with AES
cmac = MbedTLS::CMAC.new(:aes_128, "16-byte-key-here")
cmac.update("data")
mac = cmac.finish
cmac.free
```

### Cipher Operations (Encryption/Decryption)

```ruby
# AES encryption
cipher = MbedTLS::Cipher.new(:aes_128_cbc)
cipher.set_key("16-byte-key-here", :encrypt)
cipher.set_iv("16-byte-iv--here")
ciphertext = cipher.update("plaintext data")
ciphertext << cipher.finish
cipher.free

# AES decryption
decipher = MbedTLS::Cipher.new(:aes_128_cbc)
decipher.set_key("16-byte-key-here", :decrypt)
decipher.set_iv("16-byte-iv--here")
plaintext = decipher.update(ciphertext)
plaintext << decipher.finish
decipher.free
```

### Public Key Operations

```ruby
# RSA, ECC operations
pkey = MbedTLS::PKey.new
# ... (refer to sig files for detailed API)
```

## Available Modules

- **MbedTLS::Digest** - Hash functions (SHA-256, etc.)
- **MbedTLS::HMAC** - HMAC operations
- **MbedTLS::CMAC** - CMAC operations
- **MbedTLS::Cipher** - Symmetric encryption (AES, etc.)
- **MbedTLS::PKey** - Public key cryptography (RSA, ECC)

## Common Algorithms

### Digest
- `:sha256` - SHA-256 hash

### Cipher
- `:aes_128_cbc` - AES-128 in CBC mode
- `:aes_128_ecb` - AES-128 in ECB mode
- (See sig files for complete list)

## Notes

- Always call `free()` to release resources when done
- Used internally by `picoruby-net` for HTTPS/TLS
- Suitable for secure communications and data protection
- Memory-efficient implementations for embedded systems
