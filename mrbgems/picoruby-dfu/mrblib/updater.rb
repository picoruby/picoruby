require 'pack'

module DFU
  class Updater
    MAGIC = "DFU\0"
    VERSION = 1
    HEADER_SIZE = 19  # a4Ca4NNn
    HEADER_FORMAT = "a4Ca4NNn"
    CHUNK_SIZE = 4096

    # Parse the first HEADER_SIZE bytes of a receive buffer and return the total
    # expected byte count (header + optional signature + firmware body).
    # Returns nil when the buffer is too short or the magic does not match.
    # Useful for non-blocking receive loops that need to know when all data has
    # arrived before calling #receive.
    #
    # DFU header layout (HEADER_FORMAT = "a4Ca4NNn", big-endian):
    #   offset  0: magic   (4 bytes) "DFU\0"
    #   offset  4: version (1 byte)
    #   offset  5: type    (4 bytes) "RUBY" or "RITE"
    #   offset  9: fw_size (4 bytes, big-endian uint32)
    #   offset 13: crc32   (4 bytes, big-endian uint32)
    #   offset 17: sig_len (2 bytes, big-endian uint16)
    def self.expected_size(buf)
      return nil if buf.bytesize < HEADER_SIZE
      return nil unless buf.byteslice(0, 4) == MAGIC
      fw_size = ((buf.getbyte(9)  || 0) << 24) |
                ((buf.getbyte(10) || 0) << 16) |
                ((buf.getbyte(11) || 0) << 8)  |
                 (buf.getbyte(12) || 0)
      sig_len = ((buf.getbyte(17) || 0) << 8) | (buf.getbyte(18) || 0)
      HEADER_SIZE + sig_len + fw_size
    end

    def initialize(verify_crc: true, verify_signature: false, path: nil)
      @verify_crc = verify_crc
      @verify_signature = verify_signature
      @path = path
    end

    # Receive firmware from an IO-like object (must respond to #read).
    # When path: was given at initialization, writes directly to that path
    # without A/B slot management or meta file updates.
    # When path: is nil, writes to the inactive A/B slot (existing behavior).
    def receive(io)
      # Read fixed header
      puts "[recv] reading header (#{HEADER_SIZE} bytes)..."
      header = read_exact(io, HEADER_SIZE)
      got_size = header ? header.bytesize : 0
      puts "[recv] got #{got_size} bytes for header"
      unless header && got_size == HEADER_SIZE
        raise "DFU: incomplete header (expected #{HEADER_SIZE} bytes, got #{got_size})"
      end

      # @type var size: Integer
      # @type var crc32: Integer
      magic, ver, type, size, crc32, sig_len = header.unpack(HEADER_FORMAT)
      puts "[recv] type=#{type} size=#{size} crc32=0x#{crc32&.to_s(16)} sig_len=#{sig_len}"

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
        got_sig = signature ? signature.bytesize : 0
        puts "[recv] got #{got_sig} bytes for signature"
        unless signature && got_sig == sig_len
          raise "DFU: incomplete signature (expected #{sig_len} bytes, got #{got_sig})"
        end
      end

      if @path
        receive_to_path(io, size, crc32, signature)
      else
        receive_to_slot(io, size, crc32, signature, ext)
      end
    end

    private

    # Read exactly n bytes from io, looping over partial reads.
    # TCPSocket#read may return "" (empty string, not nil) when no data
    # is available yet; nil means EOF (connection closed).
    def read_exact(io, n)
      buf = ""
      while buf.bytesize < n
        chunk = io.read(n - buf.bytesize)
        break if chunk.nil?           # EOF
        next  if chunk.bytesize == 0  # no data yet, retry
        buf += chunk
      end
      buf
    end

    # Write received firmware body directly to @path.
    # No A/B slot management or meta file updates are performed.
    # The destination directory must already exist.
    # Existing files at path are overwritten.
    def receive_to_path(io, size, crc32, signature)
      path = @path.to_s
      dir = File.dirname(path)
      unless File.directory?(dir)
        raise "DFU: directory does not exist: #{dir}"
      end

      fw_path = path
      written = 0
      puts "[recv] reading firmware body (#{size} bytes) to #{fw_path}..."
      begin
        File.open(fw_path, "w") do |f|
          remaining = size || 0
          while remaining > 0
            chunk_len = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE
            puts "[recv] read_exact(#{chunk_len}) (written=#{written}/#{size})..."
            chunk = read_exact(io, chunk_len)
            puts "[recv] got #{chunk.bytesize} bytes"
            if chunk.bytesize == 0
              raise "DFU: connection lost (#{written}/#{size} bytes received)"
            end
            f.write(chunk)
            written += chunk.bytesize
            remaining -= chunk.bytesize
          end
          f.fsync
        end
      rescue => e
        File.unlink(fw_path) if File.exist?(fw_path)
        raise e
      end

      if written != size
        File.unlink(fw_path) if File.exist?(fw_path)
        raise "DFU: size mismatch (expected #{size}, got #{written})"
      end

      firmware_data = File.open(fw_path, "r") { |f| f.read }

      if @verify_crc && crc32 != 0
        actual_crc = CRC.crc32(firmware_data)
        if actual_crc != crc32
          File.unlink(fw_path) if File.exist?(fw_path)
          raise "DFU: CRC32 mismatch (expected #{crc32}, got #{actual_crc})"
        end
      end

      if @verify_signature && signature
        unless DFU.respond_to?(:ecdsa_public_key_pem)
          raise "DFU: ecdsa_public_key_pem not available (public key not embedded at build time)"
        end
        public_key = MbedTLS::PKey::EC.new(DFU.ecdsa_public_key_pem)
        digest = MbedTLS::Digest.new(:sha256)
        unless public_key.verify(digest, signature, firmware_data)
          File.unlink(fw_path) if File.exist?(fw_path)
          raise "DFU: signature verification failed"
        end
      end

      true
    end

    # Write received firmware body to the inactive A/B slot.
    # Updates meta.yml before and after writing (existing behavior).
    def receive_to_slot(io, size, crc32, signature, ext)
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
            puts "[recv] got #{chunk.bytesize} bytes"
            if chunk.bytesize == 0
              raise "DFU: connection lost (#{written}/#{size} bytes received)"
            end
            f.write(chunk)
            written += chunk.bytesize
            remaining -= chunk.bytesize
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
