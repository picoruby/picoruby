module DFU
  class Updater
    def initialize(verify_crc: true, verify_signature: false)
      @verify_crc = verify_crc
      @verify_signature = verify_signature
      @written = 0
      @size = 0
      @expected_crc32 = nil
      @expected_sig = nil
      @ext = nil
    end

    # Start an update to the inactive slot.
    # Raises on error (e.g., testing state, invalid args).
    def begin(size:, ext:, crc32: nil, signature: nil)
      meta = Meta.load

      # Reject if in testing state
      if meta["try_slot"] != meta["active_slot"]
        raise "DFU update rejected: firmware test in progress. Call DFU.confirm or DFU.rollback first."
      end

      unless ext == "mrb" || ext == "rb"
        raise "DFU: ext must be 'mrb' or 'rb'"
      end

      @target_slot = Meta.inactive_slot(meta)
      @size = size
      @ext = ext
      @expected_crc32 = crc32
      @expected_sig = signature
      @written = 0

      # Clean up existing firmware files in target slot
      ["mrb", "rb"].each do |e|
        path = "#{ENV['HOME']}/app_#{@target_slot}.#{e}"
        File.unlink(path) if File.exist?(path)
      end

      # Mark slot as updating
      slot_data = Meta.slot(meta, @target_slot)
      slot_data["state"] = "updating"
      slot_data["ext"] = @ext
      slot_data["crc32"] = nil
      slot_data["sig"] = nil
      Meta.save(meta)

      # Open file for writing
      @path = "#{ENV['HOME']}/app_#{@target_slot}.#{@ext}"
      @file = File.open(@path, "w")

      @target_slot
    end

    # Write a chunk of firmware data.
    def write(data)
      raise "DFU: update not started" unless @file
      n = @file.write(data)
      @written += n
      n
    end

    # Finalize the update: close file, verify, update meta.
    def commit
      raise "DFU: update not started" unless @file
      @file.fsync
      @file.close
      @file = nil

      if @written != @size
        cleanup_on_failure
        raise "DFU: size mismatch (expected #{@size}, got #{@written})"
      end

      firmware_data = File.open(@path, "r") { |f| f.read }

      # CRC32 verification
      if @verify_crc && @expected_crc32
        actual_crc = CRC.crc32(firmware_data)
        if actual_crc != @expected_crc32
          cleanup_on_failure
          raise "DFU: CRC32 mismatch (expected #{@expected_crc32}, got #{actual_crc})"
        end
      end

      # Signature verification
      if @verify_signature && @expected_sig
        verify_signature(firmware_data, @expected_sig) # steep:ignore
      end

      # Update meta: mark slot as ready, set try_slot
      meta = Meta.load
      slot_data = Meta.slot(meta, @target_slot)
      slot_data["state"] = "ready"
      slot_data["ext"] = @ext
      slot_data["crc32"] = @expected_crc32 || CRC.crc32(firmware_data)
      slot_data["sig"] = @expected_sig
      meta["try_slot"] = @target_slot
      meta["boot_count"] = 0
      Meta.save(meta)

      true
    end

    private

    def cleanup_on_failure
      if @file
        @file.close
        @file = nil
      end
      File.unlink(@path) if File.exist?(@path)

      # Revert slot state to empty
      meta = Meta.load
      slot_data = Meta.slot(meta, @target_slot)
      slot_data["state"] = "empty"
      slot_data["ext"] = nil
      slot_data["crc32"] = nil
      slot_data["sig"] = nil
      Meta.save(meta)
    end

    def verify_signature(firmware_data, sig_base64)
      unless DFU.respond_to?(:ecdsa_public_key_pem)
        raise "DFU: ecdsa_public_key_pem not available (public key not embedded at build time)"
      end

      signature = Base64.decode64(sig_base64)
      public_key = MbedTLS::PKey::EC.new(DFU.ecdsa_public_key_pem)
      digest = MbedTLS::Digest.new(:sha256)
      unless public_key.verify(digest, signature, firmware_data)
        cleanup_on_failure
        raise "DFU: signature verification failed"
      end
    end
  end
end
