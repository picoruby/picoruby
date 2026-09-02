class JSBinaryTest < Picotest::Test
  def setup
    JS.eval(<<~JS)
      globalThis.picorubyBinaryBlob = function() {
        return new Blob([new Uint8Array([0x00, 0x7f, 0x80, 0xff, 0x0a])]);
      };
      globalThis.picorubyNotBinary = function() {
        return {};
      };
    JS
  end

  def test_blob_to_binary_preserves_every_byte
    binary = JS.global.picorubyBinaryBlob.to_binary

    assert_equal(5, binary.bytesize)
    assert_equal(0x00, binary.getbyte(0))
    assert_equal(0x7f, binary.getbyte(1))
    assert_equal(0x80, binary.getbyte(2))
    assert_equal(0xff, binary.getbyte(3))
    assert_equal(0x0a, binary.getbyte(4))
  end

  def test_to_binary_rejects_an_object_without_array_buffer
    assert_raise(RuntimeError, 'JS object does not implement arrayBuffer()') do
      JS.global.picorubyNotBinary.to_binary
    end
  end
end
