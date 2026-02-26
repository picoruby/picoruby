require 'pack'

module DFU
  class Updater
    MAGIC = "DFU\0"
    VERSION = 1
    HEADER_SIZE = 19  # a4Ca4NNn
    HEADER_FORMAT = "a4Ca4NNn"
    CHUNK_SIZE = 4096

    def initialize(verify_crc: true, verify_signature: false)
      @verify_crc = verify_crc
      @verify_signature = verify_signature
    end

    # Receive firmware from an IO-like object (must respond to #read).
    # Reads binary header, then streams body to the inactive slot.
    def receive(io)
      # Read fixed header
      puts "[recv] reading header (#{HEADER_SIZE} bytes)..."
      header = read_exact(io, HEADER_SIZE)
      got_size = header ? header.size : 0
      puts "[recv] got #{got_size} bytes for header"
      unless header && got_size == HEADER_SIZE
        raise "DFU: incomplete header (expected #{HEADER_SIZE} bytes, got #{got_size})"
      end

      magic, ver, type, size, crc32, sig_len = header.unpack(HEADER_FORMAT)
      puts "[recv] type=#{type} size=#{size} crc32=0x#{crc32.to_s(16)} sig_len=#{sig_len}"

      unless magic == MAGIC
        # @type var magic: String
        hex = magic.unpack("C4").map { |b| b.to_s(16) }.join(" ")
        raise "DFU: invalid magic (got 0x#{hex}, expected \"DFU\\0\")"
      end
      unless ver == VERSION
        raise "DFU: unsupported protocol version (got #{ver}, expected #{VERSION})"
      end
      unless type == "RUBY" || type == "RITE"
        raise "DFU: invalid type (got \"#{type}\", expected \"RUBY\" or \"RITE\")"
      end
      if sig_len.nil? || sig_len < 0
        raise "DFU: invalid signature length (got #{sig_len})"
      end

      ext = (type == "RITE") ? "mrb" : "rb"

      # Read optional signature (raw bytes)
      signature = nil
      if sig_len > 0
        puts "[recv] reading signature (#{sig_len} bytes)..."
        signature = read_exact(io, sig_len)
        got_sig = signature ? signature.size : 0
        puts "[recv] got #{got_sig} bytes for signature"
        unless signature && got_sig == sig_len
          raise "DFU: incomplete signature (expected #{sig_len} bytes, got #{got_sig})"
        end
      end

      # Prepare inactive slot
      meta = Meta.load
      if meta["try_slot"] != meta["active_slot"]
        raise "DFU: firmware test in progress (active=#{meta['active_slot']}, try=#{meta['try_slot']}). Call DFU.confirm or DFU.rollback first."
      end

      target_slot = Meta.inactive_slot(meta)

      # Clean up existing firmware files in target slot
      ["mrb", "rb"].each do |e|
        path = "#{ENV['HOME']}/app_#{target_slot}.#{e}"
        File.unlink(path) if File.exist?(path)
      end

      # Mark slot as updating
      slot_data = Meta.slot(meta, target_slot)
      slot_data["state"] = "updating"
      slot_data["ext"] = ext
      slot_data["crc32"] = nil
      slot_data["sig"] = nil
      Meta.save(meta)

      # Stream body to file
      fw_path = "#{ENV['HOME']}/app_#{target_slot}.#{ext}"
      written = 0
      puts "[recv] reading firmware body (#{size} bytes)..."
      begin
        File.open(fw_path, "w") do |f|
          remaining = size || 0
          while remaining > 0
            chunk_len = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE
            puts "[recv] read_exact(#{chunk_len}) (written=#{written}/#{size})..."
            chunk = read_exact(io, chunk_len)
            puts "[recv] got #{chunk.size} bytes"
            if chunk.size == 0
              raise "DFU: connection lost (#{written}/#{size} bytes received)"
            end
            f.write(chunk)
            written += chunk.size
            remaining -= chunk.size
          end
          f.fsync
        end
      rescue => e
        cleanup_on_failure(fw_path, target_slot)
        raise e
      end

      if written != size
        cleanup_on_failure(fw_path, target_slot)
        raise "DFU: size mismatch (expected #{size}, got #{written})"
      end

      firmware_data = File.open(fw_path, "r") { |f| f.read }

      # CRC32 verification
      if @verify_crc && crc32 != 0
        actual_crc = CRC.crc32(firmware_data)
        if actual_crc != crc32
          cleanup_on_failure(fw_path, target_slot)
          raise "DFU: CRC32 mismatch (expected #{crc32}, got #{actual_crc})"
        end
      end

      # Signature verification
      if @verify_signature && signature
        verify_signature(firmware_data, signature, fw_path, target_slot)
      end

      # Update meta: mark slot as ready, set try_slot
      meta = Meta.load
      slot_data = Meta.slot(meta, target_slot)
      slot_data["state"] = "ready"
      slot_data["ext"] = ext
      slot_data["crc32"] = (crc32 != 0) ? crc32 : CRC.crc32(firmware_data)
      slot_data["sig"] = nil
      meta["try_slot"] = target_slot
      meta["boot_count"] = 0
      Meta.save(meta)

      true
    end

    private

    # Read exactly n bytes from io, looping over partial reads.
    # TCPSocket#read may return "" (empty string, not nil) when no data
    # is available yet; nil means EOF (connection closed).
    def read_exact(io, n)
      buf = ""
      while buf.size < n
        chunk = io.read(n - buf.size)
        break if chunk.nil?       # EOF
        next  if chunk.size == 0  # no data yet, retry
        buf += chunk
      end
      buf
    end

    def cleanup_on_failure(fw_path, target_slot)
      File.unlink(fw_path) if File.exist?(fw_path)

      meta = Meta.load
      slot_data = Meta.slot(meta, target_slot)
      slot_data["state"] = "empty"
      slot_data["ext"] = nil
      slot_data["crc32"] = nil
      slot_data["sig"] = nil
      Meta.save(meta)
    end

    def verify_signature(firmware_data, signature, fw_path, target_slot)
      unless DFU.respond_to?(:ecdsa_public_key_pem)
        raise "DFU: ecdsa_public_key_pem not available (public key not embedded at build time)"
      end

      public_key = MbedTLS::PKey::EC.new(DFU.ecdsa_public_key_pem)
      digest = MbedTLS::Digest.new(:sha256)
      unless public_key.verify(digest, signature, firmware_data)
        cleanup_on_failure(fw_path, target_slot)
        raise "DFU: signature verification failed"
      end
    end
  end
end
