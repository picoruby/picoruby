class JSNameTest < Picotest::Test
  # Property keys and method names go to JS with their byte length, as
  # ruby.wasm does. A name with an embedded NUL must reach JS in full and
  # must not hit the property that its prefix names.
  def setup
    JS.eval(<<~JS)
      globalThis.picorubyNameHolder = {};
      globalThis["picorubyNameFn\\u0000x"] = function(a) {
        return "called:" + (a === undefined ? "none" : a);
      };
      globalThis.picorubyNameFn = function() { return "prefix"; };
    JS
  end

  def test_aset_and_aref_with_string_key
    holder = JS.global[:picorubyNameHolder]
    holder["a\x00b"] = "v"
    assert_equal("v", holder["a\x00b"])
    assert_equal('{"a\\u0000b":"v"}', JS.eval('JSON.stringify(globalThis.picorubyNameHolder)'))
  end

  def test_aset_with_symbol_key
    holder = JS.global[:picorubyNameHolder]
    holder[:"c\x00d"] = 1
    assert_equal('{"c\\u0000d":1}', JS.eval('JSON.stringify(globalThis.picorubyNameHolder)'))
  end

  # JS::Object is a BasicObject, so these use __send__ (it has no #send).
  def test_method_name_with_embedded_nul
    assert_equal("called:none", JS.global.__send__(:"picorubyNameFn\x00x"))
    assert_equal("called:s", JS.global.__send__(:"picorubyNameFn\x00x", "s"))
    assert_equal("called:7", JS.global.__send__(:"picorubyNameFn\x00x", 7))
  end

  def test_setter_name_with_embedded_nul
    JS.global[:picorubyNameHolder].__send__(:"p\x00q=", 5)
    assert_equal('{"p\\u0000q":5}', JS.eval('JSON.stringify(globalThis.picorubyNameHolder)'))
  end

  # The setter used to copy the name into a 100-byte stack buffer.
  def test_setter_with_long_name
    name = "x" * 150
    JS.global[:picorubyNameHolder].__send__("#{name}=".to_sym, 9)
    assert_equal(9, JS.eval("globalThis.picorubyNameHolder.#{name}"))
  end

  def test_hash_keys_with_embedded_nul
    JS::Object.register_callback('picotestHashNul') { |_value| { "a\x00b" => 1, :"c\x00d" => 2 } }
    result = JS.eval("JSON.stringify(globalThis.picorubyGenericCallbacks['picotestHashNul'](1))")
    assert_equal('{"a\\u0000b":1,"c\\u0000d":2}', result)
  end
end
