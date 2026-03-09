require 'pack'

module DFU
  class Updater
    MAGIC = "DFU\0"
    VERSION = 1
    HEADER_SIZE = 19  # a4Ca4NNn
    HEADER_FORMAT = "a4Ca4NNn"
    CHUNK_SIZE = 4096
    READ_TIMEOUT_SEC = 5.0
    READ_POLL_MS = 10

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

    def initialize(verify_crc: true, verify_signature: false, path: nil, read_timeout_sec: READ_TIMEOUT_SEC, read_poll_ms: READ_POLL_MS)
      @verify_crc = verify_crc
      @verify_signature = verify_signature
      @path = path
      @read_timeout_sec = read_timeout_sec
      @read_poll_ms = read_poll_ms
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

    # Read the entire firmware body into RAM.
    # Reads in CHUNK_SIZE pieces to allow timeout handling per chunk,
    # but does not perform any flash I/O until all data is received.
    def read_firmware_body(io, size)
      buf = ""
      remaining = size || 0
      while remaining > 0
        chunk_len = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE
        puts "[recv] read_exact(#{chunk_len}) (received=#{buf.bytesize}/#{size})..."
        chunk = read_exact(io, chunk_len)
        puts "[recv] got #{chunk.bytesize} bytes"
        if chunk.bytesize == 0
          raise "DFU: connection lost (#{buf.bytesize}/#{size} bytes received)"
        end
        buf += chunk
        remaining -= chunk.bytesize
      end
      if buf.bytesize != size
        raise "DFU: size mismatch (expected #{size}, got #{buf.bytesize})"
      end
      buf
    end

    # Read exactly n bytes from io, looping over partial reads.
    # Prefer non-blocking read when available so timeout can be enforced
    # even on STDIN-backed transports.
    # TCPSocket#read may return "" (empty string, not nil) when no data
    # is available yet; nil means EOF (connection closed).
    # Returns partial data when timeout is reached.
    def read_exact(io, n)
      buf = ""
      deadline = Time.now.to_f + @read_timeout_sec
      use_nonblock = io.respond_to?(:read_nonblock)
      if io == STDIN && !use_nonblock
        raise "DFU: STDIN.read_nonblock is unavailable; cannot enforce receive timeout"
      end
      while buf.bytesize < n
        chunk = if use_nonblock
                  io.read_nonblock(n - buf.bytesize)
                else
                  io.read(n - buf.bytesize)
                end
        if chunk.nil?
          if use_nonblock
            if Time.now.to_f >= deadline
              puts "[recv] timeout waiting for #{n - buf.bytesize} bytes"
              break
            end
            sleep_ms @read_poll_ms
            next
          end
          break # EOF for blocking read
        end
        if chunk.bytesize == 0        # no data yet, retry
          if Time.now.to_f >= deadline
            puts "[recv] timeout waiting for #{n - buf.bytesize} bytes"
            break
          end
          sleep_ms @read_poll_ms
          next
        end
        buf += chunk
        deadline = Time.now.to_f + @read_timeout_sec
      end
      buf
    end

    # Write received firmware body directly to @path.
    # No A/B slot management or meta file updates are performed.
    # The destination directory must already exist.
    # Existing files at path are overwritten.
    #
    # All firmware data is buffered in RAM before writing to flash.
    # This prevents data loss caused by flash erase/write operations
    # disabling interrupts and blocking USB CDC reception.
    def receive_to_path(io, size, crc32, signature)
      path = @path.to_s
      dir = File.dirname(path)
      unless File.directory?(dir)
        raise "DFU: directory does not exist: #{dir}"
      end

      fw_path = path
      puts "[recv] reading firmware body (#{size} bytes) to #{fw_path}..."
      firmware_data = read_firmware_body(io, size)

      if @verify_crc && crc32 != 0
        actual_crc = CRC.crc32(firmware_data)
        if actual_crc != crc32
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
          raise "DFU: signature verification failed"
        end
      end

      begin
        File.open(fw_path, "w") do |f|
          f.write(firmware_data)
          f.fsync
        end
      rescue => e
        File.unlink(fw_path) if File.exist?(fw_path)
        raise e
      end

      true
    end

    # Write received firmware body to the inactive A/B slot.
    # Updates meta.yml before and after writing (existing behavior).
    #
    # All firmware data is buffered in RAM before writing to flash.
    # This prevents data loss caused by flash erase/write operations
    # disabling interrupts and blocking USB CDC reception.
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

      fw_path = "#{ENV['HOME']}/app_#{target_slot}.#{ext}"
      puts "[recv] reading firmware body (#{size} bytes)..."
      firmware_data = read_firmware_body(io, size)

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

      # Write to flash after all data is received and verified
      begin
        File.open(fw_path, "w") do |f|
          f.write(firmware_data)
          f.fsync
        end
      rescue => e
        cleanup_on_failure(fw_path, target_slot)
        raise e
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
