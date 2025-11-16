class TOMLTest < Picotest::Test
  def test_parse_simple_key_value
    toml = 'title = "TOML Example"'
    result = TOML.parse(toml)
    assert_equal("TOML Example", result["title"])
  end

  def test_parse_integer
    toml = 'port = 8080'
    result = TOML.parse(toml)
    assert_equal(8080, result["port"])
  end

  def test_parse_float
    toml = 'pi = 3.14159'
    result = TOML.parse(toml)
    assert((result["pi"] - 3.14159).abs < 0.00001)
  end

  def test_parse_boolean_true
    toml = 'enabled = true'
    result = TOML.parse(toml)
    assert_equal(true, result["enabled"])
  end

  def test_parse_boolean_false
    toml = 'enabled = false'
    result = TOML.parse(toml)
    assert_equal(false, result["enabled"])
  end

  def test_parse_basic_string
    toml = 'message = "Hello, World!"'
    result = TOML.parse(toml)
    assert_equal("Hello, World!", result["message"])
  end

  def test_parse_string_with_escapes
    toml = 'path = "C:\\\\Users\\\\name"'
    result = TOML.parse(toml)
    assert_equal("C:\\Users\\name", result["path"])
  end

  def test_parse_literal_string
    toml = "path = 'C:\\Users\\name'"
    result = TOML.parse(toml)
    assert_equal('C:\\Users\\name', result["path"])
  end

  def test_parse_array_integers
    toml = 'ports = [8001, 8002, 8003]'
    result = TOML.parse(toml)
    assert_equal([8001, 8002, 8003], result["ports"])
  end

  def test_parse_array_strings
    toml = 'hosts = ["alpha", "beta", "gamma"]'
    result = TOML.parse(toml)
    assert_equal(["alpha", "beta", "gamma"], result["hosts"])
  end

  def test_parse_array_mixed
    toml = 'data = [1, "two", 3.0]'
    result = TOML.parse(toml)
    assert_equal(1, result["data"][0])
    assert_equal("two", result["data"][1])
    assert((result["data"][2] - 3.0).abs < 0.001)
  end

  def test_parse_empty_array
    toml = 'empty = []'
    result = TOML.parse(toml)
    assert_equal([], result["empty"])
  end

  def test_parse_table
    toml = <<~TOML
      [database]
      host = "localhost"
      port = 5432
    TOML

    result = TOML.parse(toml)
    assert_equal("localhost", result["database"]["host"])
    assert_equal(5432, result["database"]["port"])
  end

  def test_parse_nested_table
    toml = <<~TOML
      [server.database]
      host = "localhost"
      port = 5432
    TOML

    result = TOML.parse(toml)
    assert_equal("localhost", result["server"]["database"]["host"])
    assert_equal(5432, result["server"]["database"]["port"])
  end

  def test_parse_multiple_tables
    toml = <<~TOML
      [server]
      host = "localhost"

      [database]
      name = "mydb"
    TOML

    result = TOML.parse(toml)
    assert_equal("localhost", result["server"]["host"])
    assert_equal("mydb", result["database"]["name"])
  end

  def test_parse_array_of_tables
    toml = <<~TOML
      [[products]]
      name = "Hammer"
      sku = 738594937

      [[products]]
      name = "Nail"
      sku = 284758393
    TOML

    result = TOML.parse(toml)
    assert_equal(2, result["products"].length)
    assert_equal("Hammer", result["products"][0]["name"])
    assert_equal(738594937, result["products"][0]["sku"])
    assert_equal("Nail", result["products"][1]["name"])
    assert_equal(284758393, result["products"][1]["sku"])
  end

  def test_parse_with_comments
    toml = <<~TOML
      # This is a comment
      title = "TOML" # inline comment
      # Another comment
    TOML

    result = TOML.parse(toml)
    assert_equal("TOML", result["title"])
  end

  def test_parse_integer_with_underscores
    toml = 'count = 1_000_000'
    result = TOML.parse(toml)
    assert_equal(1000000, result["count"])
  end

  def test_parse_hex_integer
    toml = 'color = 0xFF0000'
    result = TOML.parse(toml)
    assert_equal(16711680, result["color"])
  end

  def test_parse_octal_integer
    toml = 'permissions = 0o755'
    result = TOML.parse(toml)
    assert_equal(493, result["permissions"])
  end

  def test_parse_binary_integer
    toml = 'flags = 0b11010110'
    result = TOML.parse(toml)
    assert_equal(214, result["flags"])
  end

  def test_parse_negative_integer
    toml = 'temperature = -15'
    result = TOML.parse(toml)
    assert_equal(-15, result["temperature"])
  end

  def test_parse_negative_float
    toml = 'temperature = -15.5'
    result = TOML.parse(toml)
    assert((result["temperature"] - (-15.5)).abs < 0.001)
  end

  def test_parse_float_with_exponent
    toml = 'avogadro = 6.022e23'
    result = TOML.parse(toml)
    assert((result["avogadro"] - 6.022e23).abs < 1e20)
  end

  def test_generate_simple_key_value
    data = {"title" => "TOML Example"}
    result = TOML.generate(data)
    assert(result.include?('title = "TOML Example"'))
  end

  def test_generate_integer
    data = {"port" => 8080}
    result = TOML.generate(data)
    assert(result.include?("port = 8080"))
  end

  def test_generate_float
    data = {"pi" => 3.14}
    result = TOML.generate(data)
    assert(result.include?("pi = 3.14"))
  end

  def test_generate_boolean
    data = {"enabled" => true, "debug" => false}
    result = TOML.generate(data)
    assert(result.include?("enabled = true"))
    assert(result.include?("debug = false"))
  end

  def test_generate_array
    data = {"ports" => [8001, 8002, 8003]}
    result = TOML.generate(data)
    assert(result.include?("ports = [8001, 8002, 8003]"))
  end

  def test_generate_table
    data = {
      "database" => {
        "host" => "localhost",
        "port" => 5432
      }
    }
    result = TOML.generate(data)
    assert(result.include?("[database]"))
    assert(result.include?('host = "localhost"'))
    assert(result.include?("port = 5432"))
  end

  def test_generate_array_of_tables
    data = {
      "products" => [
        {"name" => "Hammer", "sku" => 738594937},
        {"name" => "Nail", "sku" => 284758393}
      ]
    }
    result = TOML.generate(data)
    assert(result.include?("[[products]]"))
    assert(result.include?('name = "Hammer"'))
    assert(result.include?('name = "Nail"'))
  end

  def test_parse_generate_roundtrip
    original = {
      "title" => "Config",
      "server" => {
        "host" => "localhost",
        "port" => 8080
      }
    }

    toml = TOML.generate(original)
    parsed = TOML.parse(toml)

    assert_equal("Config", parsed["title"])
    assert_equal("localhost", parsed["server"]["host"])
    assert_equal(8080, parsed["server"]["port"])
  end

  def test_parse_complex_document
    toml = <<~TOML
      title = "TOML Example"

      [owner]
      name = "Tom"
      age = 30

      [database]
      enabled = true
      ports = [8001, 8002, 8003]
      temp_targets = [70.0, 72.5]

      [servers.alpha]
      ip = "10.0.0.1"
      role = "frontend"

      [servers.beta]
      ip = "10.0.0.2"
      role = "backend"
    TOML

    result = TOML.parse(toml)
    assert_equal("TOML Example", result["title"])
    assert_equal("Tom", result["owner"]["name"])
    assert_equal(30, result["owner"]["age"])
    assert_equal(true, result["database"]["enabled"])
    assert_equal([8001, 8002, 8003], result["database"]["ports"])
    assert_equal("10.0.0.1", result["servers"]["alpha"]["ip"])
    assert_equal("backend", result["servers"]["beta"]["role"])
  end

  def test_parse_empty_document
    toml = ""
    result = TOML.parse(toml)
    assert_equal({}, result)
  end

  def test_parse_only_comments
    toml = <<~TOML
      # Just comments
      # Nothing else
    TOML

    result = TOML.parse(toml)
    assert_equal({}, result)
  end
end
