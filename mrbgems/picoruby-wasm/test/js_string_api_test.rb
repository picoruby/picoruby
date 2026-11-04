class JSStringAPITest < Picotest::Test
  # These APIs used to take their String arguments as C strings, so a String
  # with an embedded NUL raised ArgumentError. They now pass the byte length,
  # as ruby.wasm does. Node has no DOM and a real WebSocket would connect, so
  # setup installs fakes that record what JS receives. Each test file runs in
  # its own Node process, so the fakes do not reach other tests.
  def setup
    JS.eval(<<~JS)
      globalThis.Element = class {
        constructor(tag) { this.tag = tag; this.attrs = {}; }
        setAttribute(name, value) { this.attrs[name] = value; }
        removeAttribute(name) { delete this.attrs[name]; }
      };
      globalThis.document = new globalThis.Element("#document");
      globalThis.document.createElement = (tag) => new globalThis.Element(tag);
      globalThis.document.createTextNode = (text) => ({ text: text });
      globalThis.WebSocket = class {
        static OPEN = 1;
        constructor(url) {
          this.url = url;
          this.readyState = 1;
          this.sent = [];
          globalThis.picorubyLastSocket = this;
        }
        send(data) { this.sent.push(data); }
      };
    JS
  end

  def test_eval_keeps_embedded_nul
    assert_equal(3, JS.eval("'a\x00b'.length"))
  end

  def test_create_element_and_text_node_keep_embedded_nul
    doc = JS.global[:document]
    assert_equal(true, doc.is_a?(JS::Element))
    assert_equal("x\x00y", doc.createElement("x\x00y")[:tag])
    assert_equal("t\x00u", doc.createTextNode("t\x00u")[:text])
  end

  def test_set_and_remove_attribute_keep_embedded_nul
    doc = JS.global[:document]
    doc.setAttribute("n\x00m", "v\x00w")
    assert_equal('{"n\\u0000m":"v\\u0000w"}', JS.eval('JSON.stringify(globalThis.document.attrs)'))
    doc.removeAttribute("n\x00m")
    assert_equal('{}', JS.eval('JSON.stringify(globalThis.document.attrs)'))
  end

  def test_websocket_keeps_embedded_nul
    ws = JS::WebSocket.new("ws://example.invalid/a\x00b")
    ws.send("m\x00n")
    ws.binaryType = "x\x00y"
    result = JS.eval(<<~JS)
      JSON.stringify([globalThis.picorubyLastSocket.url,
                      globalThis.picorubyLastSocket.sent,
                      globalThis.picorubyLastSocket.binaryType])
    JS
    assert_equal('["ws://example.invalid/a\\u0000b",["m\\u0000n"],"x\\u0000y"]', result)
  end

  def test_event_listener_type_keeps_embedded_nul
    target = JS.global[:EventTarget].new
    log = []
    target.addEventListener("e\x00f", sync: true) { |_ev| log << :fired }
    target.dispatchEvent(JS.global[:Event].new("e\x00f"))
    target.dispatchEvent(JS.global[:Event].new("e"))
    assert_equal([:fired], log)
  end

  def test_register_callback_name_keeps_embedded_nul
    JS::Object.register_callback("picotestName\x00x") { |_value| 5 }
    assert_equal(5, JS.eval("globalThis.picorubyGenericCallbacks['picotestName\\u0000x'](0)"))
  end
end
