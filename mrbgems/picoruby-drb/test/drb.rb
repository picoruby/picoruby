class DRbTest < Picotest::Test
  # Test object for server
  class TestService
    def hello(name)
      "Hello, #{name}!"
    end

    def add(a, b)
      a + b
    end

    def echo(arg)
      arg
    end

    def raise_error
      raise "test error"
    end
  end

  def test_drb_uri_parsing
    uri = "druby://localhost:8787"
    server = DRb::DRbServer.new(uri, TestService.new)
    assert_equal uri, server.uri
  end

  def test_drb_message_format
    # Test Marshal round-trip for dRuby message
    msg = [nil, :hello, ["world"], nil]
    data = Marshal.dump(msg)
    result = Marshal.load(data)
    assert_equal msg, result
  end

  def test_drb_object_creation
    obj = DRb::DRbObject.new("druby://localhost:8787", :test_ref)
    assert_equal "druby://localhost:8787", obj.uri
    assert_equal :test_ref, obj.ref
  end

  def test_drb_object_equality
    obj1 = DRb::DRbObject.new("druby://localhost:8787", :ref1)
    obj2 = DRb::DRbObject.new("druby://localhost:8787", :ref1)
    obj3 = DRb::DRbObject.new("druby://localhost:8787", :ref2)

    assert_equal obj1, obj2
    assert obj1 != obj3
  end
end
