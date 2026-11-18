class Base64Test < Picotest::Test
  def setup
    # require the library to be tested using its absolute path
    require "base64"
  end

  def test_encode
    assert_equal "cGljb3J1Ynk=\n", Base64.encode64("picoruby")
    assert_equal "", Base64.encode64("")
  end

  def test_encode_breaks_lines_every_60_characters
    encoded = Base64.encode64("a" * 70)
    assert_equal "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh\nYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYQ==\n", encoded
    assert_equal "a" * 70, Base64.decode64(encoded)
  end

  def test_strict_encode
    assert_equal "cGljb3J1Ynk=", Base64.strict_encode64("picoruby")
    assert_equal "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYQ==", Base64.strict_encode64("a" * 70)
  end

  def test_decode
    assert_equal "picoruby", Base64.decode64("cGljb3J1Ynk=")
    assert_equal "picoruby", Base64.decode64("cGljb3J1\nYnk=\n")
    assert_equal "picoruby", Base64.decode64("cGlj b3J1 Ynk")
    assert_equal "", Base64.decode64("")
  end

  # The cases below give what CRuby's decode64 gives: padding ends the
  # input, and a '=' before the third character of a group is skipped
  def test_decode_stops_at_padding
    assert_equal "a", Base64.decode64("YQ==YQ==")
    assert_equal "a", Base64.decode64("YQ=\n=Yg==")
    assert_equal "a", Base64.decode64("YW=Jj")
    assert_equal "a", Base64.decode64("=YQ==")
    assert_equal "a", Base64.decode64("Y Q = =")
    assert_equal "abcd", Base64.decode64("YWJjZA=")
    assert_equal "", Base64.decode64("Y")
  end

  def test_encode_line_break_positions
    assert_equal "YWFh" * 15 + "\n", Base64.encode64("a" * 45)
    assert_equal "YWFh" * 15 + "\nYQ==\n", Base64.encode64("a" * 46)
  end

  def test_strict_decode_rejects_what_cruby_rejects
    assert_equal "", Base64.strict_decode64("")
    assert_equal "ab", Base64.strict_decode64("YWI=")
    assert_raise(ArgumentError) { Base64.strict_decode64("YR==") }
    assert_raise(ArgumentError) { Base64.strict_decode64("YWJ=") }
    assert_raise(ArgumentError) { Base64.strict_decode64("YWJj\n") }
    assert_raise(ArgumentError) { Base64.strict_decode64(" YWJj") }
    assert_raise(ArgumentError) { Base64.strict_decode64("YQ==YQ==") }
    assert_raise(ArgumentError) { Base64.strict_decode64("YQ=A") }
  end

  # mruby-encoding gives force_encoding on the mruby VM, so a decoded
  # text reads as UTF-8 again
  def test_decoded_text_reads_as_utf8_after_force_encoding
    return if RUBY_ENGINE == "mruby/c"
    s = Base64.decode64("44GC")
    assert_equal 3, s.size
    assert_equal 1, s.force_encoding("UTF-8").size
  end

  # The decoded string holds bytes: an index counts them even where they
  # would read as a UTF-8 character ("\xE3\x81\x82") or as none ("\xFF").
  # mruby/c strings have no binary encoding to give it.
  def test_decode_gives_bytes
    return if RUBY_ENGINE == "mruby/c"
    s = Base64.decode64("44GC/2Fi")
    assert_equal 6, s.size
    assert_equal 0xE3, s[0].ord
    assert_equal "ab", s[4, 2]
    assert_equal 6, Base64.urlsafe_decode64("44GC_2Fi").size
  end

  def test_strict_decode
    assert_equal "picoruby", Base64.strict_decode64("cGljb3J1Ynk=")
    assert_raise(ArgumentError) { Base64.strict_decode64("cGljb3J1Ynk=\n") }
    assert_raise(ArgumentError) { Base64.strict_decode64("cGljb3J1Ynk") }
    assert_raise(ArgumentError) { Base64.strict_decode64("cGlj*3J1Ynk=") }
  end

  def test_urlsafe
    # bytes that are not UTF-8, built at run time so the source stays UTF-8
    fffe = 255.chr + 254.chr
    ff = 255.chr
    assert_equal "__4=", Base64.urlsafe_encode64(fffe)
    assert_equal "__4", Base64.urlsafe_encode64(fffe, padding: false)
    assert_equal fffe, Base64.urlsafe_decode64("__4=")
    assert_equal fffe, Base64.urlsafe_decode64("__4")
    assert_equal "_w", Base64.urlsafe_encode64(ff, padding: false)
    assert_equal ff, Base64.urlsafe_decode64("_w")
  end

  def test_long_string
    long_string = "This is a long string for testing base64 encoding and decoding with PicoRuby."
    encoded = Base64.encode64(long_string)
    decoded = Base64.decode64(encoded)
    assert_equal long_string, decoded
  end
end
