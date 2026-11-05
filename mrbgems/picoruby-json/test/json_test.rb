class JsonTest < Picotest::Test
  def setup
    skip "Not supported on FemtoRuby" if femtoruby?
  end

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

  def test_digger_array_index
    assert_equal(2, JSON::Digger.new('[1,2]').dig(1).parse)
    assert_equal(2, JSON::Digger.new('[1 , 2]').dig(1).parse)
    assert_equal(1, JSON::Digger.new('[ 1 ,2 ]').dig(0).parse)
    assert_raise(JSON::DiggerError) { JSON::Digger.new('[1,2]').dig(2) }
    assert_raise(JSON::DiggerError) { JSON::Digger.new('[]').dig(0) }
    assert_raise(JSON::DiggerError) { JSON::Digger.new('[[1,2]]').dig(0, 2) }
    assert_equal(7, JSON::Digger.new('[false , 7]').dig(1).parse)
    assert_equal("b", JSON::Digger.new("[\n\t\"a\"\r\n,\n\"b\"\n]").dig(1).parse)
  end

  def test_digger_whitespace_in_object
    assert_equal(2, JSON::Digger.new('{"a":{ },"b":2}').dig("b").parse)
    assert_equal(2, JSON::Digger.new('{ "a" : [ ] , "b" : 2 }').dig("b").parse)
    assert_equal(1, JSON::Digger.new("{\n\"a\"\n:\n1\n}").dig("a").parse)
  end

  def test_digger_array_after_nested_array_in_object
    assert_equal(2, JSON::Digger.new('[{"a":[1]},2]').dig(1).parse)
    assert_equal(3, JSON::Digger.new('[{"a":[1]},{ },3]').dig(2).parse)
    assert_equal("x", JSON::Digger.new('{"a":{"b":[1,2]},"c":"x"}').dig("c").parse)
  end

  def test_digger_number_with_signed_exponent
    assert_equal(150.0, JSON::Digger.new('{"a":1.5e+2}').dig("a").parse)
    assert_equal(2, JSON::Digger.new('{"a":1.5e+2,"b":2}').dig("b").parse)
    assert_equal(2, JSON::Digger.new('[1.5E-2,2]').dig(1).parse)
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

  def test_parse_number_after_multibyte
    # Regression: parse_integer/parse_float must index by character, not byte.
    # A multibyte character earlier in the string makes the character index
    # diverge from the byte index, which previously corrupted every number
    # parsed after it (e.g. message ids collapsing to duplicate garbage values).
    assert_equal([1, 2, 3],
      JSON.parse('[{"id":1,"n":"a"},{"id":2,"n":"b"},{"id":3,"n":"c"}]').map { |m| m["id"] })
    assert_equal([1, 2, 3],
      JSON.parse('[{"id":1,"n":"マン"},{"id":2,"n":"b"},{"id":3,"n":"c"}]').map { |m| m["id"] })
    # Multibyte UTF-8 bytes directly in the source string (not \u escaped).
    assert_equal([1, 2, 3],
      JSON.parse('[{"id":1,"n":"マン"},{"id":2,"n":"x"},{"id":3,"n":"y"}]').map { |m| m["id"] })
    # Nested objects with a multibyte value, mirroring real ActionCable payloads.
    nested = '[{"id":1,"u":{"n":"L-chikaマン"}},{"id":2,"u":{"n":"x"}},{"id":3,"u":{"n":"y"}}]'
    assert_equal([1, 2, 3], JSON.parse(nested).map { |m| m["id"] })
    # Floats and negatives after a multibyte character must also stay correct.
    assert_equal({"name" => "日本", "lat" => 35.5, "offset" => -7},
      JSON.parse('{"name":"日本","lat":35.5,"offset":-7}'))
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

  # ---- the CRuby API ----

  def test_parse_symbolize_names
    assert_equal({a: 1, b: {c: [2]}}, JSON.parse('{"a":1,"b":{"c":[2]}}', symbolize_names: true))
    assert_equal({"a" => 1}, JSON.parse('{"a":1}', symbolize_names: false))
    assert_equal({"a" => 1}, JSON.parse('{"a":1}', max_nesting: 10, allow_nan: true))
  end

  def test_parse_errors
    assert_raise(JSON::ParserError) { JSON.parse('{"a":') }
    assert_raise(TypeError) { JSON.parse(nil) }
  end

  def test_load_and_dump
    assert_equal({"a" => 1}, JSON.load('{"a":1}'))
    assert_nil(JSON.load(nil))
    assert_nil(JSON.load(""))
    assert_equal('{"a":1}', JSON.dump({"a" => 1}))
    assert_equal('[1,2]', JSON[[1, 2]])
    assert_equal([1, 2], JSON['[1,2]'])
  end

  def test_pretty_generate
    expected = "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    2\n  ],\n  \"c\": {}\n}"
    assert_equal(expected, JSON.pretty_generate({"a" => 1, "b" => [1, 2], "c" => {}}))
    assert_equal("[]", JSON.pretty_generate([]))
  end

  def test_generate_with_options
    assert_equal('{"a": 1}', JSON.generate({"a" => 1}, space: " "))
    state = JSON::State.new(indent: "\t", object_nl: "\n")
    assert_equal("{\n\t\"a\":1\n}", JSON.generate({"a" => 1}, state))
    assert_equal("{\n\t\"a\":1\n}", state.generate({"a" => 1}))
    assert_true(JSON::State.from_state(state).equal?(state))
    assert_equal(" ", JSON::State.from_state(space: " ").space)
  end

  def test_to_json
    assert_equal('{"a":[1,"x",null,true]}', {"a" => [1, "x", nil, true]}.to_json)
    assert_equal('"str"', "str".to_json)
    assert_equal('1', 1.to_json)
    assert_equal('null', nil.to_json)
    assert_equal('"sym"', :sym.to_json)
  end

  def test_own_to_json_is_used
    klass = Class.new do
      def to_json(*) = '{"custom":true}'
    end
    assert_equal('[{"custom":true}]', JSON.generate([klass.new]))
  end

  def test_fragment
    assert_equal('[{"raw":1}]', JSON.generate([JSON::Fragment.new('{"raw":1}')]))
  end

  def test_float_special_values
    assert_raise(JSON::GeneratorError) { JSON.generate([0.0 / 0.0]) }
    assert_equal('[NaN,Infinity,-Infinity]', JSON.generate([0.0 / 0.0, 1.0 / 0.0, -1.0 / 0.0], allow_nan: true))
  end

  def test_generate_string_escapes
    assert_equal('"a\\"b\\\\c\\n\\u0001"', JSON.generate("a\"b\\c\n\u0001"))
    assert_equal('"日本語"', JSON.generate("日本語"))
    assert_equal('"a\\/b"', JSON.generate("a/b", script_safe: true))
    assert_equal('"a/b"', JSON.generate("a/b"))
  end

  def test_nesting_error
    deep = []
    cur = deep
    120.times { nxt = []; cur << nxt; cur = nxt }
    assert_raise(JSON::NestingError) { JSON.generate(deep) }
    assert_equal(true, JSON.generate(deep, max_nesting: 0).start_with?("[[["))
  end
end
