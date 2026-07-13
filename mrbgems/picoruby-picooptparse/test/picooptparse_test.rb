class PicoOptionParserTest < Picotest::Test
  def build_parser
    parser = PicoOptionParser.new
    parser.flag("--help", "-h", help: true, desc: "show help")
    parser.flag("--no-color", default: true, desc: "disable color")
    parser.on("--mode", "-m", type: :symbol, choices: [:fast, :slow], default: :fast, desc: "run mode")
    parser.on("--name", type: :string, default: "anon")
    parser.on("--retries", type: :integer, default: 3)
    parser
  end

  def test_defaults_seeded_when_args_empty
    result = build_parser.parse([])
    assert_equal :fast, result[:mode]
    assert_equal "anon", result[:name]
    assert_equal 3, result[:retries]
    assert_equal true, result[:color]
  end

  def test_long_value_option_space_separated
    result = build_parser.parse(["--name", "taro"])
    assert_equal "taro", result[:name]
  end

  def test_inline_value_option
    result = build_parser.parse(["--name=taro"])
    assert_equal "taro", result[:name]
  end

  def test_integer_coercion
    result = build_parser.parse(["--retries", "7"])
    assert_equal 7, result[:retries]
  end

  def test_integer_zero_is_valid
    result = build_parser.parse(["--retries", "0"])
    assert_equal 0, result[:retries]
  end

  def test_bad_integer_raises
    assert_raise(ArgumentError) do
      build_parser.parse(["--retries", "abc"])
    end
  end

  def test_symbol_choice_pass
    result = build_parser.parse(["--mode", "slow"])
    assert_equal :slow, result[:mode]
  end

  def test_invalid_choice_raises
    assert_raise(ArgumentError) do
      build_parser.parse(["--mode", "bogus"])
    end
  end

  def test_negatable_boolean
    result = build_parser.parse(["--no-color"])
    assert_equal false, result[:color]
  end

  def test_boolean_with_inline_value_raises
    assert_raise(ArgumentError) do
      build_parser.parse(["--no-color=1"])
    end
  end

  def test_short_alias_with_value
    result = build_parser.parse(["-m", "slow"])
    assert_equal :slow, result[:mode]
  end

  def test_missing_value_raises
    assert_raise(ArgumentError) do
      build_parser.parse(["--name"])
    end
  end

  def test_unknown_option_raises
    assert_raise(ArgumentError) do
      build_parser.parse(["--frob"])
    end
  end

  def test_help_flag_returns_help
    assert_equal :help, build_parser.parse(["-h"])
  end

  def test_usage_contains_program_and_options
    usage = build_parser.usage("myprog")
    assert_equal true, usage.include?("Usage: myprog [options]")
    assert_equal true, usage.include?("--mode")
    assert_equal true, usage.include?("fast|slow")
    assert_equal true, usage.include?("--retries N")
  end
end
