# Quoted scalars, flow collections, ~ and :symbols, as Psych reads them.
class YAMLScalarTest < Picotest::Test
  def test_double_quoted
    doc = YAML.load(%Q{a: "can't be blank"\nb: "x\\ty \\"q\\""\nc: "a: b"\nd: "\\u00e9"\n})
    assert_equal("can't be blank", doc["a"])
    assert_equal("x\ty \"q\"", doc["b"])
    assert_equal("a: b", doc["c"])
    assert_equal("é", doc["d"])
  end

  def test_single_quoted
    doc = YAML.load("a: 'it''s'\nb: '%{count} items'\n")
    assert_equal("it's", doc["a"])
    assert_equal("%{count} items", doc["b"])
  end

  def test_flow_sequence
    doc = YAML.load("a: [~, Jan, Feb]\nb: []\nc: [1, \"x, y\"]\n")
    assert_equal([nil, "Jan", "Feb"], doc["a"])
    assert_equal([], doc["b"])
    assert_equal([1, "x, y"], doc["c"])
  end

  def test_flow_mapping
    doc = YAML.load("a: {x: 1, y: two}\n")
    assert_equal({ "x" => 1, "y" => "two" }, doc["a"])
  end

  def test_tilde_symbol_and_comment
    doc = YAML.load("a: ~\nb: :year\nc: plain # comment\n")
    assert_nil(doc["a"])
    assert_equal(:year, doc["b"])
    assert_equal("plain", doc["c"])
  end

  def test_quoted_list_item_is_a_scalar
    doc = YAML.load("a:\n  - \"k: v\"\n  - :month\n")
    assert_equal(["k: v", :month], doc["a"])
  end
end
