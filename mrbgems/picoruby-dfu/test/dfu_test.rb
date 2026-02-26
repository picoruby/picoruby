class MetaUtilTest < Picotest::Test

  description "DFU::Meta utility methods"
  def test_deep_copy
    original = {
      "key" => "value",
      "nested" => {
        "inner" => "data"
      }
    }
    copy = DFU::Meta.deep_copy(original)
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
    assert_equal "confirmed", DFU::Meta.slot(meta, "a")["state"]
    assert_equal "empty", DFU::Meta.slot(meta, "b")["state"]
  end

  def test_inactive_slot
    meta_a = { "try_slot" => "a" }
    meta_b = { "try_slot" => "b" }
    assert_equal "b", DFU::Meta.inactive_slot(meta_a)
    assert_equal "a", DFU::Meta.inactive_slot(meta_b)
  end

  def test_firmware_path_with_ext
    meta = {
      "slot_a" => { "ext" => "mrb" },
      "slot_b" => { "ext" => "rb" }
    }
    path_a = DFU::Meta.firmware_path(meta, "a")
    path_b = DFU::Meta.firmware_path(meta, "b")
    assert path_a.end_with?("/app_a.mrb")
    assert path_b.end_with?("/app_b.rb")
  end

  def test_firmware_path_nil_when_no_ext
    meta = {
      "slot_a" => { "ext" => nil }
    }
    assert_nil DFU::Meta.firmware_path(meta, "a")
  end

  def test_default_has_required_keys
    d = DFU::Meta::DEFAULT
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
  description "DFU meta.yml YAML serialization roundtrip"

  def test_roundtrip
    meta = DFU::Meta.deep_copy(DFU::Meta::DEFAULT)
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
  description "DFU.confirm and DFU.rollback"

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
    stub(DFU::Meta).load { meta }
    stub(DFU::Meta).save {}

    DFU.confirm

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
    stub(DFU::Meta).load { meta }
    stub(DFU::Meta).save {}

    DFU.rollback

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
    stub(DFU::Meta).load { meta }

    assert_raise(RuntimeError) { DFU.rollback }
  end

  def test_boot_manager_returns_nil_when_no_firmware
    meta = DFU::Meta.deep_copy(DFU::Meta::DEFAULT)
    stub(DFU::Meta).recover {}
    stub(DFU::Meta).load { meta }
    stub(DFU::Meta).save {}

    result = DFU::BootManager.resolve
    assert_nil result
  end
end

# Mock IO class for testing DFU::Updater#receive
class MockIO
  def initialize(data)
    @data = data
    @pos = 0
  end

  def read(len)
    return nil if @pos >= @data.size
    chunk = @data[@pos, len]
    @pos += chunk.size
    chunk
  end
end

class UpdaterReceiveTest < Picotest::Test
  description "DFU::Updater#receive binary protocol"

  def build_packet(body, type: "RUBY", ver: 1, crc32: 0, signature: nil)
    sig_len = signature ? signature.size : 0
    header = ["DFU\0", ver, type, body.size, crc32, sig_len].pack("a4Ca4NNn")
    header + (signature || "") + body
  end

  def stub_meta_for_update
    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "a",
      "boot_count"        => 0,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "empty",     "ext" => nil,   "crc32" => nil, "sig" => nil }
    }
    stub(DFU::Meta).load { meta }
    stub(DFU::Meta).save {}
    stub(File).exist? { false }
    stub(File).unlink {}
    meta
  end

  def test_receive_valid_ruby_constants
    assert_equal 19, DFU::Updater::HEADER_SIZE
    assert_equal "a4Ca4NNn", DFU::Updater::HEADER_FORMAT
    assert_equal 1, DFU::Updater::VERSION
  end

  def test_receive_invalid_magic
    data = "BAD\0" + "\x01RUBY" + "\x00\x00\x00\x04" + "\x00\x00\x00\x00" + "\x00\x00" + "test"
    io = MockIO.new(data)

    updater = DFU::Updater.new
    assert_raise(RuntimeError) { updater.receive(io) }
  end

  def test_receive_unsupported_version
    body = "test"
    packet = build_packet(body, ver: 99)
    io = MockIO.new(packet)

    updater = DFU::Updater.new
    assert_raise(RuntimeError) { updater.receive(io) }
  end

  def test_receive_invalid_type
    body = "test"
    sig_len = 0
    header = ["DFU\0", 1, "JUNK", body.size, 0, sig_len].pack("a4Ca4NNn")
    io = MockIO.new(header + body)

    updater = DFU::Updater.new
    assert_raise(RuntimeError) { updater.receive(io) }
  end

  def test_receive_incomplete_header
    io = MockIO.new("DFU\0\x01RU")

    updater = DFU::Updater.new
    assert_raise(RuntimeError) { updater.receive(io) }
  end

  def test_receive_rejects_during_testing_phase
    body = "test"
    packet = build_packet(body)
    io = MockIO.new(packet)

    meta = {
      "format_version"    => 1,
      "active_slot"       => "a",
      "try_slot"          => "b",
      "boot_count"        => 1,
      "max_boot_attempts" => 3,
      "slot_a" => { "state" => "confirmed", "ext" => "mrb", "crc32" => nil, "sig" => nil },
      "slot_b" => { "state" => "ready",     "ext" => "mrb", "crc32" => nil, "sig" => nil }
    }
    stub(DFU::Meta).load { meta }

    updater = DFU::Updater.new
    assert_raise(RuntimeError) { updater.receive(io) }
  end

  def test_receive_header_parse
    # Build a valid header and verify it parses correctly
    body = "test"
    packet = build_packet(body, type: "RUBY", crc32: 12345)
    header = packet[0, 19]
    magic, ver, type, size, crc32, sig_len = header.unpack("a4Ca4NNn")
    assert_equal 4, magic.size
    assert_equal 1, ver
    assert_equal "RUBY", type
    assert_equal 4, size
    assert_equal 12345, crc32
    assert_equal 0, sig_len
  end

  def test_receive_header_parse_rite
    body = "RITE0006"
    packet = build_packet(body, type: "RITE")
    header = packet[0, 19]
    magic, ver, type, size, crc32, sig_len = header.unpack("a4Ca4NNn")
    assert_equal "RITE", type
    assert_equal 8, size
  end
end
