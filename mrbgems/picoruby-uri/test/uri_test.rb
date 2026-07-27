class URITest < Picotest::Test
  # ---- URI.parse ----

  def test_parse_http_defaults
    uri = URI.parse("http://example.com")
    assert_equal("http", uri.scheme)
    assert_equal("example.com", uri.host)
    assert_equal(80, uri.port)
    assert_equal("/", uri.path)
    assert_nil(uri.query)
    assert_nil(uri.fragment)
  end

  def test_parse_https_default_port
    uri = URI.parse("https://example.com/index.html")
    assert_equal("https", uri.scheme)
    assert_equal(443, uri.port)
    assert_equal("/index.html", uri.path)
  end

  def test_parse_explicit_port_and_query_and_fragment
    uri = URI.parse("http://example.com:8080/search?q=ruby#top")
    assert_equal(8080, uri.port)
    assert_equal("/search", uri.path)
    assert_equal("q=ruby", uri.query)
    assert_equal("top", uri.fragment)
  end

  def test_request_uri_includes_query
    uri = URI.parse("http://example.com/a/b?x=1")
    assert_equal("/a/b?x=1", uri.request_uri)
  end

  def test_request_uri_defaults_to_root
    uri = URI.parse("http://example.com")
    assert_equal("/", uri.request_uri)
  end

  def test_to_s_omits_default_port
    assert_equal("http://example.com/x", URI.parse("http://example.com/x").to_s)
    assert_equal("http://example.com:8080/x", URI.parse("http://example.com:8080/x").to_s)
  end

  def test_parse_rejects_nil_and_unknown_scheme
    assert_raise(ArgumentError) { URI.parse(nil) }
    assert_raise(ArgumentError) { URI.parse("ftp://example.com") }
  end

  # ---- URI.encode_www_form_component (CRuby-compatible) ----

  def test_encode_component_passes_safe_characters
    assert_equal("AZaz09*-._", URI.encode_www_form_component("AZaz09*-._"))
  end

  def test_encode_component_space_becomes_plus
    assert_equal("hello+world", URI.encode_www_form_component("hello world"))
  end

  def test_encode_component_encodes_reserved_characters
    assert_equal("a%26b%3Dc%3F", URI.encode_www_form_component("a&b=c?"))
  end

  def test_encode_component_encodes_tilde_like_cruby
    # CRuby's form encoding percent-encodes the tilde (it is NOT in the
    # form-safe set, unlike RFC 3986 unreserved)
    assert_equal("%7E", URI.encode_www_form_component("~"))
  end

  def test_encode_component_multibyte_is_byte_wise
    # UTF-8 bytes of the hiragana "a"; the old net-http implementation
    # emitted the codepoint hex ("%3042"), which is not a valid encoding
    assert_equal("%E3%81%82", URI.encode_www_form_component("あ"))
  end

  def test_encode_component_stringifies_non_strings
    assert_equal("42", URI.encode_www_form_component(42))
    assert_equal("page", URI.encode_www_form_component(:page))
  end

  # ---- URI.encode_www_form ----

  def test_encode_www_form_from_hash
    assert_equal("page=2&q=hello+world",
      URI.encode_www_form({ page: 2, q: "hello world" }))
  end

  def test_encode_www_form_from_pairs
    assert_equal("a=1&a=2", URI.encode_www_form([["a", 1], ["a", 2]]))
  end

  def test_encode_www_form_empty
    assert_equal("", URI.encode_www_form({}))
  end
end
