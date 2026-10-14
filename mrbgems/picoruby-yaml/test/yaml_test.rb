# picoruby-yaml is a small parser and generator: a document is a Hash of
# scalars, nested Hashes and lists of scalars. The tests cover what the
# README promises; quoting, anchors and tags are out of scope.
class YAMLTest < Picotest::Test
  def test_load_scalars
    doc = YAML.load("a: 1\nb: 2.5\nc: true\nd: false\ne: null\nf: hello world\ng: -3\n")
    assert_equal(1, doc["a"])
    assert_equal(2.5, doc["b"])
    assert_equal(true, doc["c"])
    assert_equal(false, doc["d"])
    assert_nil(doc["e"])
    assert_equal("hello world", doc["f"])
    assert_equal(-3, doc["g"])
  end

  def test_load_keeps_a_string_that_only_looks_like_a_number
    doc = YAML.load("a: 1.2.3\nb: 1e3\nc: 12abc\n")
    assert_equal("1.2.3", doc["a"])
    assert_equal("1e3", doc["b"])
    assert_equal("12abc", doc["c"])
  end

  def test_load_literals_ignore_case
    doc = YAML.load("a: TRUE\nb: False\nc: Null\n")
    assert_equal(true, doc["a"])
    assert_equal(false, doc["b"])
    assert_nil(doc["c"])
  end

  def test_load_list_of_scalars
    doc = YAML.load("list:\n  - 1\n  - two\n  - 3.0\n  - false\n")
    assert_equal([1, "two", 3.0, false], doc["list"])
  end

  def test_load_nested_hashes
    doc = YAML.load("a:\n  b:\n    c: 1\n  d: 2\ne: 3\n")
    assert_equal({"a" => {"b" => {"c" => 1}, "d" => 2}, "e" => 3}, doc)
  end

  def test_load_skips_blank_lines_and_comments
    doc = YAML.load("# comment\n\nk: v\n\n# another\nz: 1\n")
    assert_equal({"k" => "v", "z" => 1}, doc)
  end

  def test_load_keeps_a_colon_inside_a_value
    doc = YAML.load("url: http://example.com/x\n")
    assert_equal("http://example.com/x", doc["url"])
  end

  def test_load_empty_value_is_an_empty_hash
    assert_equal({"a" => {}}, YAML.load("a:\n"))
    assert_equal({}, YAML.load(""))
  end

  def test_load_readme_example
    yaml = <<~YAML
      name: John Doe
      age: 30
      address:
        street: 123 Main St
        city: Anytown
        country: USA
      hobbies:
        - reading
        - cycling
        - cooking
      is_student: false
      gpa: 3.5
    YAML
    doc = YAML.load(yaml)
    assert_equal("John Doe", doc["name"])
    assert_equal(30, doc["age"])
    assert_equal({"street" => "123 Main St", "city" => "Anytown", "country" => "USA"}, doc["address"])
    assert_equal(["reading", "cycling", "cooking"], doc["hobbies"])
    assert_equal(false, doc["is_student"])
    assert_equal(3.5, doc["gpa"])
  end

  def test_dump_scalars
    yaml = YAML.dump({"n" => 1, "f" => 2.5, "t" => true, "z" => nil, "s" => "hi"})
    assert_equal("n: 1\nf: 2.5\nt: true\nz: null\ns: hi\n", yaml)
  end

  def test_dump_nested
    yaml = YAML.dump({"h" => {"k" => "v"}, "l" => [1, "a"]})
    assert_equal("h:\n  k: v\nl:\n  - 1\n  - a\n", yaml)
  end

  def test_dump_top_level_array
    assert_equal("- 1\n- 2\n", YAML.dump([1, 2]))
  end

  def test_dump_symbol_keys
    assert_equal("name: PicoRuby\nversion: 3\n", YAML.dump({name: "PicoRuby", version: 3}))
  end

  def test_round_trip
    doc = {
      "name" => "John",
      "n" => 1,
      "f" => 2.5,
      "t" => true,
      "z" => nil,
      "h" => {"k" => "v", "m" => {"deep" => 7}},
      "l" => [1, "a", 3.5, false],
    }
    assert_equal(doc, YAML.load(YAML.dump(doc)))
  end

  def test_load_file
    path = "#{ENV['TMPDIR'] || '/tmp'}/picoruby_yaml_test.yml"
    File.open(path, "w") { |f| f.write("name: PicoRuby\nversion: 3\n") }
    assert_equal({"name" => "PicoRuby", "version" => 3}, YAML.load_file(path))
    File.unlink(path)
  end
end
