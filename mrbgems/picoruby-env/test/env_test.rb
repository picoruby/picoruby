class EnvTest < Picotest::Test
  def setup
    ENV["PICORUBY_ENV_TEST_A"] = "1"
    ENV["PICORUBY_ENV_TEST_B"] = "2"
  end

  def teardown
    ENV.delete("PICORUBY_ENV_TEST_A")
    ENV.delete("PICORUBY_ENV_TEST_B")
    ENV.delete("PICORUBY_ENV_TEST_C")
  end

  def test_key_p
    assert_true ENV.key?("PICORUBY_ENV_TEST_A")
    assert_true ENV.include?("PICORUBY_ENV_TEST_A")
    assert_false ENV.has_key?("PICORUBY_ENV_TEST_C")
  end

  def test_value_p_and_key
    ENV["PICORUBY_ENV_TEST_C"] = "picoruby-env-test"
    assert_true ENV.value?("picoruby-env-test")
    assert_equal "PICORUBY_ENV_TEST_C", ENV.key("picoruby-env-test")
  end

  def test_fetch
    assert_equal "1", ENV.fetch("PICORUBY_ENV_TEST_A")
    assert_equal "d", ENV.fetch("PICORUBY_ENV_TEST_C", "d")
    assert_equal "PICORUBY_ENV_TEST_C!", ENV.fetch("PICORUBY_ENV_TEST_C") { |name| name + "!" }
  end

  def test_fetch_missing_raises
    raised = false
    begin
      ENV.fetch("PICORUBY_ENV_TEST_C")
    rescue
      raised = true
    end
    assert_true raised
  end

  def test_to_h
    h = ENV.to_h
    assert_equal "1", h["PICORUBY_ENV_TEST_A"]
    h["PICORUBY_ENV_TEST_A"] = "changed"
    assert_equal "1", ENV["PICORUBY_ENV_TEST_A"]
    assert_equal "1!", ENV.to_h { |k, v| [k, v + "!"] }["PICORUBY_ENV_TEST_A"]
  end

  def test_keys_values_size
    assert_true ENV.keys.include?("PICORUBY_ENV_TEST_A")
    assert_true ENV.values.include?("2")
    assert_equal ENV.keys.size, ENV.size
    assert_false ENV.empty?
  end

  def test_values_at_slice_assoc
    assert_equal ["1", nil], ENV.values_at("PICORUBY_ENV_TEST_A", "PICORUBY_ENV_TEST_C")
    assert_equal({"PICORUBY_ENV_TEST_B" => "2"}, ENV.slice("PICORUBY_ENV_TEST_B", "PICORUBY_ENV_TEST_C"))
    assert_equal ["PICORUBY_ENV_TEST_A", "1"], ENV.assoc("PICORUBY_ENV_TEST_A")
    assert_nil ENV.assoc("PICORUBY_ENV_TEST_C")
  end

  def test_update
    ENV.update({"PICORUBY_ENV_TEST_C" => "3"})
    assert_equal "3", ENV["PICORUBY_ENV_TEST_C"]
    ENV.update({"PICORUBY_ENV_TEST_A" => "10"}) { |name, old, new| old + new }
    assert_equal "110", ENV["PICORUBY_ENV_TEST_A"]
  end

  def test_select_reject
    selected = ENV.select { |k, v| k == "PICORUBY_ENV_TEST_A" }
    assert_equal({"PICORUBY_ENV_TEST_A" => "1"}, selected)
    rejected = ENV.reject { |k, v| k == "PICORUBY_ENV_TEST_A" }
    assert_false rejected.has_key?("PICORUBY_ENV_TEST_A")
    assert_equal "2", rejected["PICORUBY_ENV_TEST_B"]
  end

  def test_each_answers_env
    seen = []
    assert_equal ENV, ENV.each { |k, v| seen << k if k == "PICORUBY_ENV_TEST_A" }
    assert_equal ["PICORUBY_ENV_TEST_A"], seen
  end

  def test_to_s
    assert_equal "ENV", ENV.to_s
  end
end
