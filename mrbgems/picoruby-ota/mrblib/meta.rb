require 'yaml'

module OTA
  class Meta

    META_PATH     = "#{ENV['OTA_DIR']}/meta.yml"
    META_TMP_PATH = "#{ENV['OTA_DIR']}/meta_tmp.yml"

    DEFAULT = {
      "format_version"   => 1,
      "active_slot"      => "a",
      "try_slot"         => "a",
      "boot_count"       => 0,
      "max_boot_attempts" => 3,
      "slot_a" => {
        "state" => "empty",
        "ext"   => nil,
        "crc32" => nil,
        "sig"   => nil
      },
      "slot_b" => {
        "state" => "empty",
        "ext"   => nil,
        "crc32" => nil,
        "sig"   => nil
      }
    }

    # Recover from interrupted meta write.
    # Call this before any meta read.
    def self.recover
      tmp_exists  = File.exist?(META_TMP_PATH)
      meta_exists = File.exist?(META_PATH)

      if tmp_exists
        # Validate meta_tmp.yml
        valid = false
        begin
          data = YAML.load_file(META_TMP_PATH)
          valid = data.is_a?(Hash) && data["format_version"]
        rescue
          valid = false
        end

        if valid
          # meta_tmp.yml is valid — complete the transition
          File.unlink(META_PATH) if meta_exists
          File.rename(META_TMP_PATH, META_PATH)
        else
          # meta_tmp.yml is corrupt — discard it
          File.unlink(META_TMP_PATH)
        end
      end
      # If neither file exists, load will create a default
    end

    # Load meta.yml. Creates default if not found.
    def self.load
      unless File.exist?(META_PATH)
        save(deep_copy(DEFAULT))
      end
      meta_data = YAML.load_file(META_PATH)
      # @type var meta_data: Hash[String, untyped]
      return meta_data
    end

    # Save meta.yml with power-failure safety.
    # Write to tmp, fsync, delete old, rename tmp.
    def self.save(data)
      # 1. Write to meta_tmp.yml
      p META_TMP_PATH
      File.open(META_TMP_PATH, "w") do |f|
        f.write(YAML.dump(data))
        f.fsync
      end

      # 2. Delete old meta.yml
      File.unlink(META_PATH) if File.exist?(META_PATH)

      # 3. Rename tmp to meta.yml
      File.rename(META_TMP_PATH, META_PATH)
    end

    def self.deep_copy(hash)
      result = {}
      hash.each do |k, v|
        result[k] = v.is_a?(Hash) ? deep_copy(v) : v
      end
      result
    end

    # Accessor for slot data
    def self.slot(data, slot_name)
      data["slot_#{slot_name}"]
    end

    # Determine the inactive slot
    def self.inactive_slot(data)
      data["try_slot"] == "a" ? "b" : "a"
    end

    # Firmware path for a given slot
    def self.firmware_path(data, slot_name)
      slot_data = slot(data, slot_name)
      return nil unless slot_data && slot_data["ext"]
      "#{ENV['HOME']}/app_#{slot_name}.#{slot_data['ext']}"
    end
  end
end
