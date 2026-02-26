require 'yaml'

module DFU
  class Meta

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
      tmp_exists  = File.exist?(meta_tmp_path)
      meta_exists = File.exist?(meta_path)

      if tmp_exists
        # Validate meta_tmp.yml
        valid = false
        begin
          data = YAML.load_file(meta_tmp_path)
          valid = data.is_a?(Hash) && data["format_version"]
        rescue
          valid = false
        end

        if valid
          # meta_tmp.yml is valid — complete the transition
          File.unlink(meta_path) if meta_exists
          File.rename(meta_tmp_path, meta_path)
        else
          # meta_tmp.yml is corrupt — discard it
          File.unlink(meta_tmp_path)
        end
      end
      # If neither file exists, load will create a default
    end

    # Load meta.yml. Creates default if not found.
    def self.load
      unless File.exist?(meta_path)
        save(deep_copy(DEFAULT))
      end
      meta_data = YAML.load_file(meta_path)
      # @type var meta_data: Hash[String, untyped]
      return meta_data
    end

    # Save meta.yml with power-failure safety.
    # Write to tmp, fsync, delete old, rename tmp.
    def self.save(data)
      # 1. Write to meta_tmp.yml
      File.open(meta_tmp_path, "w") do |f|
        f.write(YAML.dump(data))
        f.fsync
      end

      # 2. Delete old meta.yml
      File.unlink(meta_path) if File.exist?(meta_path)

      # 3. Rename tmp to meta.yml
      File.rename(meta_tmp_path, meta_path)
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

    private

    def self.meta_path
      "#{ENV['DFU_DIR']}/meta.yml"
    end

    def self.meta_tmp_path
      "#{ENV['DFU_DIR']}/meta_tmp.yml"
    end

  end
end
