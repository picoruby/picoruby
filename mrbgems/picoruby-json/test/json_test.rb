class JsonTest < Picotest::Test
  def test_parse
    assert_equal({"a" => 1, "b" => "hello"}, JSON.parse('{"a":1,"b":"hello"}'))
    assert_equal([1, "two", true, nil], JSON.parse('[1, "two", true, null]'))
    assert_equal({"nested" => {"key" => "value"}}, JSON.parse('{"nested":{"key":"value"}}'))
    assert_equal([{"a" => 1}, {"b" => 2}], JSON.parse('[{"a":1},{"b":2}]'))
  end

  def test_generate
    assert_equal('{"a":1,"b":"hello"}', JSON.generate({a: 1, b: "hello"}))
    assert_equal('[1,"two",true,null]', JSON.generate([1, "two", true, nil]))
    assert_equal('{"nested":{"key":"value"}}', JSON.generate({nested: {key: "value"}}))
  end

  def test_digger
    json = '{"user":{"name":"Taro","age":30},"posts":[{"title":"Post 1","body":"..."},{"title":"Post 2","body":"..."}]}'
    digger = JSON::Digger.new(json)
    assert_equal("Taro", digger.dig("user", "name").parse)
    digger = JSON::Digger.new(json)
    assert_equal(30, digger.dig("user", "age").parse)
    digger = JSON::Digger.new(json)
    assert_equal("Post 2", digger.dig("posts", 1, "title").parse)
  end

  def test_parse_with_escaped_characters
    assert_equal('"', JSON.parse('"\\""'))
    assert_equal('\\', JSON.parse('"\\\\"'))
    assert_equal('/', JSON.parse('"\\/"'))
    assert_equal("\b", JSON.parse('"\\b"'))
    assert_equal("\f", JSON.parse('"\\f"'))
    assert_equal("\n", JSON.parse('"\\n"'))
    assert_equal("\r", JSON.parse('"\\r"'))
    assert_equal("\t", JSON.parse('"\\t"'))
  end

  def test_generate_with_special_characters
    assert_equal('"\\"hello\\""', JSON.generate('"hello"'))
    assert_equal('"\\\\"', JSON.generate('\\'))
    assert_equal('"\\b"', JSON.generate("\b"))
    assert_equal('"\\f"', JSON.generate("\f"))
    assert_equal('"\\n"', JSON.generate("\n"))
    assert_equal('"\\r"', JSON.generate("\r"))
    assert_equal('"\\t"', JSON.generate("\t"))
  end

  def test_generate_nested_json_string
    # Test for ActionCable-like nested JSON
    inner = JSON.generate({channel: "ChatChannel", room: "lobby"})
    outer = JSON.generate({command: "subscribe", identifier: inner})
    # Should be valid JSON
    parsed = JSON.parse(outer)
    assert_equal("subscribe", parsed["command"])
    # identifier should be a JSON string that can be parsed again
    identifier_parsed = JSON.parse(parsed["identifier"])
    assert_equal("ChatChannel", identifier_parsed["channel"])
    assert_equal("lobby", identifier_parsed["room"])
  end
end
