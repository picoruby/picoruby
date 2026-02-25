class MetaUtilTest < Picotest::Test

  description "OTA::Meta utility methods"
  def test_deep_copy
    original = {
      "key" => "value",
      "nested" => {
        "inner" => "data"
      }
    }
    copy = OTA::Meta.deep_copy(original)
    assert_equal "value", copy["key"]
    assert_equal "data", copy["nested"]["inner"]
    copy["nested"]["inner"] = "modified"
    assert_equal "data", original["nested"]["inner"]
  end

  def test_slot
    meta = {
      "slot_a" => { "state" => "confirmed" },
      "slot_b" => { "state" => "empty" }
    }
    assert_equal "confirmed", OTA::Meta.slot(meta, "a")["state"]
    assert_equal "empty", OTA::Meta.slot(meta, "b")["state"]
  end

  def test_inactive_slot
    meta_a = { "try_slot" => "a" }
    meta_b = { "try_slot" => "b" }
    assert_equal "b", OTA::Meta.inactive_slot(meta_a)
    assert_equal "a", OTA::Meta.inactive_slot(meta_b)
  end

  def test_firmware_path_with_ext
    meta = {
      "slot_a" => { "ext" => "mrb" },
      "slot_b" => { "ext" => "rb" }
    }
    path_a = OTA::Meta.firmware_path(meta, "a")
    path_b = OTA::Meta.firmware_path(meta, "b")
    assert path_a.end_with?("/app_a.mrb")
    assert path_b.end_with?("/app_b.rb")
  end

  def test_firmware_path_nil_when_no_ext
    meta = {
      "slot_a" => { "ext" => nil }
    }
    assert_nil OTA::Meta.firmware_path(meta, "a")
  end

  def test_default_has_required_keys
    d = OTA::Meta::DEFAULT
    assert_equal 1, d["format_version"]
    assert_equal "a", d["active_slot"]
    assert_equal "a", d["try_slot"]
    assert_equal 0, d["boot_count"]
    assert_equal 3, d["max_boot_attempts"]
    assert_equal "empty", d["slot_a"]["state"]
    assert_equal "empty", d["slot_b"]["state"]
  end
end

class MetaYamlRoundtripTest < Picotest::Test
  description "OTA meta.yml YAML serialization roundtrip"

  def test_roundtrip
    meta = OTA::Meta.deep_copy(OTA::Meta::DEFAULT)
    meta["active_slot"] = "b"
    meta["try_slot"] = "b"
    meta["boot_count"] = 2
    meta["slot_b"]["state"] = "confirmed"
    meta["slot_b"]["ext"] = "mrb"
    meta["slot_b"]["crc32"] = 1234567890

    yaml_str = YAML.dump(meta)
    loaded = YAML.load(yaml_str)

    assert_equal "b", loaded["active_slot"]
    assert_equal "b", loaded["try_slot"]
    assert_equal 2, loaded["boot_count"]
    assert_equal "confirmed", loaded["slot_b"]["state"]
    assert_equal "mrb", loaded["slot_b"]["ext"]
    assert_equal 1234567890, loaded["slot_b"]["crc32"]
  end
end

class OtaConfirmTest < Picotest::Test
  description "OTA.confirm and OTA.rollback"

  def test_confirm_sets_active_and_resets_boot_count
    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "b",
      "boot_count"        => 2,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "ready",     "ext" => "mrb", "crc32" => nil, "sig" => nil }
    }
    stub(OTA::Meta).load { meta }
    stub(OTA::Meta).save {}

    OTA.confirm

    assert_equal "b", meta["active_slot"]
    assert_equal 0, meta["boot_count"]
    assert_equal "confirmed", meta["slot_b"]["state"]
  end

  def test_rollback_resets_try_slot
    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "b",
      "boot_count"        => 1,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "ready",     "ext" => "mrb", "crc32" => nil, "sig" => nil }
    }
    stub(OTA::Meta).load { meta }
    stub(OTA::Meta).save {}

    OTA.rollback

    assert_equal "a", meta["try_slot"]
    assert_equal 0, meta["boot_count"]
  end

  def test_rollback_raises_when_already_on_active
    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "a",
      "boot_count"        => 0,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "empty",     "ext" => nil,   "crc32" => nil, "sig" => nil }
    }
    stub(OTA::Meta).load { meta }

    assert_raise(RuntimeError) { OTA.rollback }
  end

  def test_boot_manager_returns_nil_when_no_firmware
    meta = OTA::Meta.deep_copy(OTA::Meta::DEFAULT)
    stub(OTA::Meta).recover {}
    stub(OTA::Meta).load { meta }
    stub(OTA::Meta).save {}

    result = OTA::BootManager.resolve
    assert_nil result
  end
end

class UpdaterTest < Picotest::Test
  description "OTA::Updater basic validation"

  def test_rejects_update_during_testing_phase
    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "b",
      "boot_count"        => 1,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "ready",     "ext" => "mrb", "crc32" => nil, "sig" => nil }
    }
    stub(OTA::Meta).load { meta }

    updater = OTA::Updater.new
    assert_raise(RuntimeError) do
      updater.begin(size: 100, ext: "mrb")
    end
  end

  def test_rejects_invalid_ext
    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "a",
      "boot_count"        => 0,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "empty",     "ext" => nil,   "crc32" => nil, "sig" => nil }
    }
    stub(OTA::Meta).load { meta }

    updater = OTA::Updater.new
    assert_raise(RuntimeError) do
      updater.begin(size: 100, ext: "exe")
    end
  end

  def test_write_raises_before_begin
    updater = OTA::Updater.new
    assert_raise(RuntimeError) { updater.write("data") }
  end

  def test_commit_raises_before_begin
    updater = OTA::Updater.new
    assert_raise(RuntimeError) { updater.commit }
  end
end
