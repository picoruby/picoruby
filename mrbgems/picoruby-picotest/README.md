## Usage

- Any test file must end with `_test.rb`
- Any test method must start with `test_`

```ruby
require 'picotest'

class SampleTest < PicoTest::Test
  def test_truethy
    assert(false) # => fail
  end

  def test_equal
    assert_equal(1, 1) # => pass
  end

  def test_not_equal
    assert_not_equal(1, 2) # => pass
  end

  def test_nil
    assert_nil(nil) # => pass
  end

  def test_raise
    assert_raise(StandardError) { raise StandardError } # => pass
    assert_raise(StandardError, "right message") do
      raise StandardError, "wrong message"
    end # => fail
  end
end

PicoTest::Runner.run("/absolute/path/to/test_dir")
```

### Setup and Teardown

```ruby
require 'picotest'

class SampleTest < PicoTest::Test
  def setup
    @http_client = Net::HTTPClient.new('http://example.com')
  end

  def teardown
    @http_client.close
  end

  def test_get
    api = API.new(@http_client)
    assert_equal('Hello, World!', api.get)
  end
end
```

### Stub and Mock

- `stub` is a method to stub a method of an instance
- `mock` is a method to mock a method of a class
- `stub_any_instance_of` is a method to stub a method of an instance
- `mock_any_instance_of` is a method to mock a method of an instance

```ruby
require 'picotest'

class SampleTest < PicoTest::Test
  def test_stub
    http_client = Net::HTTPClient.new('http://example.com')
    stub(http_client).get('/') { 'Hello, World!' }
    api = API.new(http_client)
    assert_equal('Hello, World!', api.get)
  end
end

PicoTest::Runner.run("/absolute/path/to/test_dir")
```

```ruby
require 'picotest'

class SampleTest < PicoTest::Test
  def test_mock
    mock_any_instance_of(GPIO).read(2) { 1 } # Expect to be called with 2
    assert_equal(1, GPIO.new.read)
    # => fail because `GPIO#read` is called just once
  end
end

PicoTest::Runner.run("/absolute/path/to/test_dir")
```
