# picoruby-ota

OTA (Over-The-Air) update manager for R2P2.
A/B slot management with power-failure safety and automatic rollback.

## Overview

- **A/B slot management**: Two firmware slots (`app_a`, `app_b`) for safe updates
- **Power-failure safe**: Uses `meta_tmp.yml` pattern for crash-resistant metadata writes
- **Automatic rollback**: Falls back to previous firmware after repeated boot failures
- **Signature verification** (optional): ECDSA (secp256r1 / NIST P-256) via MbedTLS
- **CRC32 verification** (optional): Integrity check via picoruby-crc
- **Transport-agnostic**: Core logic is independent of the communication layer (BLE, stdin, etc.)

## Directory Layout (R2P2)

```
/etc/ota/
  meta.yml          # OTA state management
  meta_tmp.yml      # Temporary file for power-failure safety

/home/
  app.mrb / app.rb       # Legacy app (takes priority over OTA slots)
  app_a.mrb / app_a.rb   # Slot A firmware
  app_b.mrb / app_b.rb   # Slot B firmware
```

## Usage

### Boot (automatic)

`r2p2.rb` automatically uses OTA when no `/home/app.{mrb|rb}` exists:

1. Check `/home/app.mrb` → load if exists
2. Check `/home/app.rb` → load if exists
3. Call `OTA::BootManager.resolve` → load the appropriate slot

### Confirm Boot

The app **must** call `OTA.confirm` after successful startup to reset the boot counter:

```ruby
OTA.confirm
```

Without this call, the boot counter keeps incrementing and will trigger a rollback after `max_boot_attempts`.

### Perform an Update

```ruby
require 'ota'

updater = OTA::Updater.new(verify_crc: true, verify_signature: false)

# Start update (writes to the inactive slot)
updater.begin(size: firmware_size, ext: "mrb", crc32: expected_crc32)

# Write firmware data (can be called multiple times for chunked transfer)
updater.write(chunk1)
updater.write(chunk2)

# Finalize: verify integrity and update meta.yml
updater.commit

# Reboot to activate the new firmware
```

### Check Status

```ruby
meta = OTA.status
puts meta["active_slot"]  #=> "a"
puts meta["try_slot"]     #=> "b"
puts meta["boot_count"]   #=> 0
```

### Manual Rollback

```ruby
OTA.rollback
```

## meta.yml Format

```yaml
format_version: 1
active_slot: a
try_slot: a
boot_count: 0
max_boot_attempts: 3
slot_a:
  state: confirmed
  ext: mrb
  crc32: 3456789012
  sig: null
slot_b:
  state: empty
  ext: null
  crc32: null
  sig: null
```

### Slot States

| State | Description |
|---|---|
| `empty` | No firmware |
| `updating` | Download in progress (may be incomplete) |
| `ready` | Written and verified, awaiting boot test |
| `confirmed` | Boot-tested and confirmed by app |

## Signature Verification

### Generate ECDSA Key Pair

```bash
# Generate private key
openssl ecparam -genkey -name prime256v1 -noout -out ota_ecdsa_private.pem

# Extract public key
openssl ec -in ota_ecdsa_private.pem -pubout -out ota_ecdsa_public.pem
```

Place the keys in `mrbgems/picoruby-ota/keys/`:
- `ota_ecdsa_public.pem` — embedded into the build as a read-only C function (committed to git)
- `ota_ecdsa_private.pem` — keep secret (gitignored)

The public key is available at runtime via `OTA.ecdsa_public_key_pem` (read-only class method defined in C).

### Sign Firmware (on development machine)

```ruby
# CRuby + OpenSSL
require 'openssl'
require 'base64'

private_key = OpenSSL::PKey::EC.new(File.read("ota_ecdsa_private.pem"))
firmware = File.binread("app.mrb")
signature = private_key.sign("SHA256", firmware)
sig_base64 = Base64.strict_encode64(signature)
puts sig_base64
```

### Update with Signature

```ruby
updater = OTA::Updater.new(verify_crc: true, verify_signature: true)
updater.begin(size: size, ext: "mrb", crc32: crc32, signature: sig_base64)
updater.write(firmware_data)
updater.commit
```

## Implementing a Transport Layer

Transport layers (BLE, stdin, HTTP, etc.) should use `OTA::Updater`:

```ruby
# Example: stdin transport
require 'ota'

updater = OTA::Updater.new(verify_crc: true)
# Parse header from your protocol
updater.begin(size: size, ext: "mrb", crc32: crc32)

remaining = size
while remaining > 0
  chunk = STDIN.read([remaining, 4096].min)
  updater.write(chunk)
  remaining -= chunk.length
end

updater.commit
puts "OTA update complete. Reboot to activate."
```

## Dependencies

- `picoruby-yaml` — meta.yml parsing
- `picoruby-vfs` — file operations
- `picoruby-crc` — CRC32 verification
- `picoruby-mbedtls` — ECDSA signature verification (optional, required only when `verify_signature: true`)
