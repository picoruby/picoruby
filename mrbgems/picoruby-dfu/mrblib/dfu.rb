module DFU
  # Confirm the current boot.
  # Call this from the app after successful startup.
  def self.confirm
    meta = Meta.load
    meta["active_slot"] = meta["try_slot"]
    meta["boot_count"] = 0
    slot_data = Meta.slot(meta, meta["try_slot"])
    slot_data["state"] = "confirmed"
    Meta.save(meta)
    true
  end

  # Get current DFU status.
  def self.status
    Meta.recover
    Meta.load
  end

  # Manual rollback to active_slot.
  def self.rollback
    meta = Meta.load
    if meta["try_slot"] == meta["active_slot"]
      raise "DFU: already on active slot, nothing to rollback"
    end
    meta["try_slot"] = meta["active_slot"]
    meta["boot_count"] = 0
    Meta.save(meta)
    true
  end
end
