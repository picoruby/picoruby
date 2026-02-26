module DFU
  module BootManager
    # Select the firmware to boot.
    # Returns the firmware file path, or nil if no firmware available.
    def self.resolve
      # 1. Recover from interrupted meta write
      Meta.recover

      # 2. Load meta
      meta = Meta.load

      try_slot  = meta["try_slot"]
      active    = meta["active_slot"]
      max       = meta["max_boot_attempts"]
      count     = meta["boot_count"]
      slot_data = Meta.slot(meta, try_slot)

      # 3. Validate try_slot firmware
      path = Meta.firmware_path(meta, try_slot)
      slot_valid = path &&
                   slot_data &&
                   (slot_data["state"] == "ready" || slot_data["state"] == "confirmed") &&
                   File.exist?(path)

      # 4. Rollback check
      if !slot_valid || count >= max
        if try_slot != active
          # Rollback to active_slot
          meta["try_slot"] = active
          meta["boot_count"] = 0
          Meta.save(meta)
          return resolve  # Re-resolve with updated meta
        end

        if !slot_valid
          # Active slot is also invalid — no firmware available
          meta["boot_count"] = 0
          Meta.save(meta)
          return nil
        end

        # Same slot exceeded max but no alternative — reset and retry
        meta["boot_count"] = 0
        Meta.save(meta)
        count = 0
      end

      # 5. Increment boot_count BEFORE loading firmware
      meta["boot_count"] = count + 1
      Meta.save(meta)

      # 6. Return firmware path
      Meta.firmware_path(meta, meta["try_slot"])
    end
  end
end
