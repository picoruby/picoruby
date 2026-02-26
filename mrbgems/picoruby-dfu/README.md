# picoruby-dfu

DFU (Device Firmware Update) update manager for R2P2.
A/B slot management with power-failure safety and automatic rollback.

## Overview

- **A/B slot management**: Two firmware slots (`app_a`, `app_b`) for safe updates
- **Power-failure safe**: Uses `meta_tmp.yml` pattern for crash-resistant metadata writes
- **Automatic rollback**: Falls back to previous firmware after repeated boot failures
- **Signature verification** (optional): ECDSA (secp256r1 / NIST P-256) via MbedTLS
- **CRC32 verification** (optional): Integrity check via picoruby-crc
- **Transport-agnostic**: Single `receive(io)` API works with any IO-like object (TCP, BLE, etc.)

## Binary Header Protocol

The `receive(io)` method reads a fixed 19-byte binary header followed by optional signature and firmware body.

```
Offset  Size     Field      Description
0x00    4 bytes  magic      "DFU\0"
0x04    1 byte   version    protocol version (uint8, initially 1)
0x05    4 bytes  type       "RUBY" or "RITE" (ASCII)
0x09    4 bytes  size       firmware body size (big-endian uint32)
0x0D    4 bytes  crc32      CRC32 (big-endian uint32, 0 if none)
0x11    2 bytes  sig_len    signature length (big-endian uint16, 0 if none)
0x13    N bytes  signature  raw ECDSA DER bytes (N = sig_len)
0x13+N  ...      body       firmware data (size bytes)
```

Pack format: `"a4Ca4NNn"` (19 bytes fixed header)

- `RUBY`: source code (.rb)
- `RITE`: compiled bytecode (.mrb)

## Directory Layout (R2P2)

```
/etc/dfu/
  meta.yml          # DFU state management
  meta_tmp.yml      # Temporary file for power-failure safety

/home/
  app.mrb / app.rb       # Legacy app (takes priority over DFU slots)
  app_a.mrb / app_a.rb   # Slot A firmware
  app_b.mrb / app_b.rb   # Slot B firmware
```

## Usage

### Boot (automatic)

`r2p2.rb` automatically uses DFU when no `/home/app.{mrb|rb}` exists:

1. Check `/home/app.mrb` -> load if exists
2. Check `/home/app.rb` -> load if exists
3. Call `DFU::BootManager.resolve` -> load the appropriate slot

### Confirm Boot

The app **must** call `DFU.confirm` after successful startup to reset the boot counter:

```ruby
DFU.confirm
```

Without this call, the boot counter keeps incrementing and will trigger a rollback after `max_boot_attempts`.

### Receive an Update

```ruby
require 'dfu'

updater = DFU::Updater.new(verify_crc: true, verify_signature: false)

# io can be any object with a #read method (TCPSocket, BLE::UART, etc.)
updater.receive(io)

# Reboot to activate the new firmware
```

### TCP Transport (dfutcp shell command)

On the device:

```
dfutcp 4649
```

This starts a TCP server on port 4649 that accepts DFU binary protocol connections.

### Sending Firmware (from host)

```ruby
# CRuby sender example
require 'socket'
require 'zlib'

firmware = File.binread("app.mrb")
type = "RITE"  # or "RUBY" for .rb source
crc32 = Zlib.crc32(firmware)
sig_len = 0

header = ["DFU\0", 1, type, firmware.size, crc32, sig_len].pack("a4Ca4NNn")

sock = TCPSocket.new("192.168.1.100", 4649)
sock.write(header)
sock.write(firmware)
sock.close
```

### Check Status

```ruby
meta = DFU.status
puts meta["active_slot"]  #=> "a"
puts meta["try_slot"]     #=> "b"
puts meta["boot_count"]   #=> 0
```

### Manual Rollback

```ruby
DFU.rollback
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
openssl ecparam -genkey -name prime256v1 -noout -out dfu_ecdsa_private.pem

# Extract public key
openssl ec -in dfu_ecdsa_private.pem -pubout -out dfu_ecdsa_public.pem
```

Place the keys in `mrbgems/picoruby-dfu/keys/`:
- `dfu_ecdsa_public.pem` -- embedded into the build as a read-only C function (committed to git)
- `dfu_ecdsa_private.pem` -- keep secret (gitignored)

The public key is available at runtime via `DFU.ecdsa_public_key_pem` (read-only class method defined in C).

### Sign and Send with Signature

```ruby
# CRuby sender with signature
require 'socket'
require 'openssl'
require 'zlib'

private_key = OpenSSL::PKey::EC.new(File.read("dfu_ecdsa_private.pem"))
firmware = File.binread("app.mrb")
signature = private_key.sign("SHA256", firmware)
crc32 = Zlib.crc32(firmware)

header = ["DFU\0", 1, "RITE", firmware.size, crc32, signature.size].pack("a4Ca4NNn")

sock = TCPSocket.new("192.168.1.100", 4649)
sock.write(header)
sock.write(signature)
sock.write(firmware)
sock.close
```

On the device, use `verify_signature: true`:

```ruby
updater = DFU::Updater.new(verify_crc: true, verify_signature: true)
updater.receive(io)
```

## Dependencies

- `picoruby-yaml` -- meta.yml parsing
- `picoruby-vfs` -- file operations
- `picoruby-crc` -- CRC32 verification
- `picoruby-pack` / `mruby-pack` -- binary header parsing
- `picoruby-mbedtls` -- ECDSA signature verification (optional, required only when `verify_signature: true`)
