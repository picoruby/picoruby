# picoruby-yaml is a small parser and generator: a document is a Hash of
# scalars, nested Hashes and lists of scalars. The tests cover what the
# README promises, and anchors, aliases and << merge keys; tags are out
# of scope.
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

  def test_load_top_level_list
    assert_equal([1, "two", true], YAML.load("- 1\n- two\n- true\n"))
  end

  def test_load_list_of_hashes
    yaml = "items:\n  - name: x\n    v: 1\n  - name: y\n    v: 2\nafter: 3\n"
    doc = YAML.load(yaml)
    assert_equal([{"name" => "x", "v" => 1}, {"name" => "y", "v" => 2}], doc["items"])
    assert_equal(3, doc["after"])
  end

  def test_load_list_of_hashes_with_a_nested_value
    yaml = "- name: x\n  address:\n    city: Anytown\n  tags:\n    - a\n    - b\n- name: y\n"
    doc = YAML.load(yaml)
    assert_equal(2, doc.size)
    assert_equal("x", doc[0]["name"])
    assert_equal({"city" => "Anytown"}, doc[0]["address"])
    assert_equal(["a", "b"], doc[0]["tags"])
    assert_equal({"name" => "y"}, doc[1])
  end

  def test_load_list_item_keeps_a_url
    assert_equal(["http://x/y"], YAML.load("- http://x/y\n"))
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

  def test_round_trip_nested_lists_and_hash_items
    doc = {"grid" => [[1, 2], [3]], "items" => [{"q" => 1, "r" => "s"}, {"q" => 2}]}
    assert_equal(doc, YAML.load(YAML.dump(doc)))
  end

  # In the test binary that carries every gem, picoruby-vfs routes File to
  # a mounted volume, and nothing is mounted unless a test mounts it. Mount
  # a RAM littlefs at "/" then, as the vfs tests do; without vfs the host's
  # File reads the host filesystem.
  def test_load_file
    if Object.const_defined?(:VFS)
      skip("no Littlefs in this binary") unless Object.const_defined?(:Littlefs)
      unless VFS::VOLUMES.any? { |v| v[:mountpoint] == "/" }
        fs = Littlefs.new(:flash, label: "YAMLTEST")
        fs.mkfs
        VFS.mount(fs, "/")
      end
      path = "/picoruby_yaml_test.yml"
    else
      path = "#{ENV['TMPDIR'] || '/tmp'}/picoruby_yaml_test.yml"
    end
    File.open(path, "w") { |f| f.write("name: PicoRuby\nversion: 3\n") }
    assert_equal({"name" => "PicoRuby", "version" => 3}, YAML.load_file(path))
    File.unlink(path)
  end

  def test_alias_of_a_scalar
    doc = YAML.load("a: &x 1\nb: *x\n")
    assert_equal({"a" => 1, "b" => 1}, doc)
  end

  def test_anchor_on_a_block_mapping_keeps_its_nesting
    doc = YAML.load("default: &default\n  adapter: sqlite3\n  timeout: 5000\nother: 1\n")
    assert_equal({"default" => {"adapter" => "sqlite3", "timeout" => 5000}, "other" => 1}, doc)
  end

  def test_alias_is_the_anchored_object
    doc = YAML.load("a: &x\n  k: v\nb: *x\n")
    # object_id, as mruby/c has no equal?
    assert_equal(doc["a"].object_id, doc["b"].object_id)
  end

  def test_anchor_on_a_block_list
    doc = YAML.load("a: &l\n  - 1\n  - 2\nb: *l\n")
    assert_equal([1, 2], doc["b"])
  end

  def test_merge_key
    yaml = "default: &default\n  adapter: sqlite3\n  timeout: 5000\n\n" \
           "development:\n  <<: *default\n  database: dev\n  timeout: 1\n"
    doc = YAML.load(yaml)
    assert_equal({"adapter" => "sqlite3", "timeout" => 1, "database" => "dev"}, doc["development"])
    assert_equal({"adapter" => "sqlite3", "timeout" => 5000}, doc["default"])
  end

  def test_merge_key_with_a_list_the_first_wins
    doc = YAML.load("a: &a\n  x: 1\nb: &b\n  x: 2\n  y: 2\nc:\n  <<: [*a, *b]\n")
    assert_equal({"x" => 1, "y" => 2}, doc["c"])
  end

  def test_aliases_in_list_items
    doc = YAML.load("v: &v 3\nl:\n  - *v\n  - name: *v\n")
    assert_equal([3, {"name" => 3}], doc["l"])
  end

  def test_unknown_alias
    assert_raise(ArgumentError) { YAML.load("a: *nope\n") }
  end

  def test_dump_then_load
    values = ["Cafe #2", "*guest", "&x", "[lab] 5G", "{x}", "a: b", "ends:", " lead", "trail ",
              "", "true", "1", "1.5", "~", "null", ":sym", "#hash", "- x", "-",
              "line\nbreak", "tab\there", "q\"uote", "back\\slash", "plain text", "日本語"]
    values.each do |v|
      assert_equal({"ssid" => v}, YAML.load(YAML.dump({"ssid" => v})))
      assert_equal([v], YAML.load(YAML.dump([v])))
    end
    keys = {"a: b" => 1, "x:y" => 2, "-k" => 3, "true" => 4, "plain" => 5}
    assert_equal(keys, YAML.load(YAML.dump(keys)))
  end

  # Psych reads these as numbers, booleans, dates or times
  def test_dump_quotes_what_psych_reads_as_another_value
    values = ["yes", "No", "ON", "off", "0x10", "+1", "1_000", "0b1", ".5", "1.",
              "12:30", "2026-01-01", "5G lab", "-x"]
    values.each do |v|
      assert_equal("v: \"#{v}\"\n", YAML.dump({"v" => v}))
      assert_equal({"v" => v}, YAML.load(YAML.dump({"v" => v})))
    end
    assert_equal("v: lab 5G\n", YAML.dump({"v" => "lab 5G"}))
  end

  def test_list_item_with_a_quoted_key
    assert_equal([{"name" => "foo"}], YAML.load(%Q(- "name": foo\n)))
    assert_equal([{"a:b" => 1}], YAML.load(%Q(- 'a:b': 1\n)))
    assert_equal(["a: b"], YAML.load(%Q(- "a: b"\n)))
  end

  def test_text_after_a_closing_bracket
    assert_equal({"msg" => "[INFO] started"}, YAML.load("msg: [INFO] started\n"))
    assert_equal({"msg" => "{a} b"}, YAML.load("msg: {a} b\n"))
    assert_equal({"a" => [1, 2]}, YAML.load("a: [1, 2] # two\n"))
  end

  def test_a_symbol_starts_with_a_letter_or_underscore
    assert_equal({"port" => ":8080", "host" => "::1", "sym" => :abc, "u" => :_x},
                 YAML.load("port: :8080\nhost: ::1\nsym: :abc\nu: :_x\n"))
  end

  def test_invalid_unicode_escape
    assert_raise(ArgumentError) { YAML.load(%Q(a: "\\U00110000"\n)) }
    assert_raise(ArgumentError) { YAML.load(%Q(a: "\\uD83D"\n)) }
  end

  def test_flow_sequence_trailing_comma
    assert_equal({"a" => ["x", "y"], "b" => []}, YAML.load("a: [x, y,]\nb: []\n"))
  end

  def test_merge_key_in_a_flow_mapping
    assert_equal({"a" => {"k" => 1, "m" => 2}}, YAML.load("a: {k: 1, <<: {m: 2}}\n"))
  end
end
