class JSStringArgsTest < Picotest::Test
  # These argument shapes go through build_args_json, the JSON-encoded
  # dispatch that the one/two-string fast paths do not use. Each JS helper
  # returns JSON.stringify of what it received, so the assertions compare
  # the exact JS string values without relying on the conversion back.
  def setup
    JS.eval(<<~JS)
      globalThis.picorubyStringArgs3 = function(a, b, n) {
        return JSON.stringify([a, b, n]);
      };
      globalThis.picorubyStringArgs4 = function(a, b, c, d) {
        return JSON.stringify([a, b, c, d]);
      };
      globalThis.picorubyStringArgsFn = function(a, b) {
        return JSON.stringify([a, b]);
      };
      globalThis.PicorubyStringArgsHolder = class {
        constructor(a, b) { this.value = JSON.stringify([a, b]); }
      };
    JS
  end

  def test_method_with_newline_like_insert_at_cursor
    result = JS.global.picorubyStringArgs3("src", "\nR9: ", 0)
    assert_equal('["src","\\nR9: ",0]', result)
  end

  def test_method_with_short_escapes
    result = JS.global.picorubyStringArgs3("a\tb\rc\nd", "\b\f", 1)
    assert_equal('["a\\tb\\rc\\nd","\\b\\f",1]', result)
  end

  def test_method_with_quote_and_backslash
    result = JS.global.picorubyStringArgs3("say \"hi\"", "C:\\tmp", 2)
    assert_equal('["say \\"hi\\"","C:\\\\tmp",2]', result)
  end

  def test_method_with_other_control_characters
    result = JS.global.picorubyStringArgs3("\x00\x01", "\x1b\x1f", 3)
    assert_equal('["\\u0000\\u0001","\\u001b\\u001f",3]', result)
  end

  def test_method_with_multibyte_utf8
    result = JS.global.picorubyStringArgs3("日本語", "改行\nあり", 4)
    assert_equal('["日本語","改行\\nあり",4]', result)
  end

  def test_method_with_four_arguments
    result = JS.global.picorubyStringArgs4("a\nb", "\t", "\"\\", "日本")
    assert_equal('["a\\nb","\\t","\\"\\\\","日本"]', result)
  end

  def test_constructor_with_control_characters
    holder = JS.global[:PicorubyStringArgsHolder].new("line1\nline2", "\t日本語")
    assert_equal('["line1\\nline2","\\t日本語"]', holder[:value])
  end

  def test_function_call_with_control_characters
    result = JS.global[:picorubyStringArgsFn].call("x\ny", "\"q\"\\")
    assert_equal('["x\\ny","\\"q\\"\\\\"]', result)
  end
end
