class JWTTest < Picotest::Test
  SECRET = "my$ecretK3"

  # Made by CRuby (OpenSSL::HMAC, Base64url with no padding) from
  # {"sub":"dev","raw":"<U+3042>","esc":"<U+3042 as a JSON escape>",
  # "exp":4102444800}: "raw" holds the UTF-8 bytes, "esc" the escape
  TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." \
          "eyJzdWIiOiJkZXYiLCJyYXciOiLjgYIiLCJlc2MiOiJcdTMwNDIiLCJleHAiOjQxMDI0NDQ4MDB9." \
          "3JGwV2eYIB6blUlufDZXA6-R-7GPB_OcxWsoSZIY2Ec"
  # The same token with the last character of the signature changed
  TAMPERED = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." \
             "eyJzdWIiOiJkZXYiLCJyYXciOiLjgYIiLCJlc2MiOiJcdTMwNDIiLCJleHAiOjQxMDI0NDQ4MDB9." \
             "3JGwV2eYIB6blUlufDZXA6-R-7GPB_OcxWsoSZIY2EA"

  # picoruby-json, which JWT parses with, does not support FemtoRuby
  # (its own tests skip it): a \u escape there needs Array#pack
  def setup
    skip "JSON is not supported on FemtoRuby" if femtoruby?
    require "jwt"
  end

  def test_decode_a_token_made_by_cruby
    payload, header = JWT.decode(TOKEN, SECRET, algorithm: "HS256")
    assert_equal "HS256", header["alg"]
    assert_equal "dev", payload["sub"]
    assert_equal 4102444800, payload["exp"]
  end

  def test_tampered_signature_fails
    assert_raise(JWT::VerificationError) do
      JWT.decode(TAMPERED, SECRET, algorithm: "HS256")
    end
  end

  def test_segment_that_is_not_base64url_is_a_decode_error
    assert_raise(JWT::DecodeError) do
      JWT.decode("e!!.e30.x", SECRET, algorithm: "HS256")
    end
  end

  # RFC 7515: Base64url with no padding
  def test_encode_writes_no_padding
    token = JWT.encode({"sub" => "d", "exp" => 4102444800}, SECRET, "HS256")
    assert_false token.include?("=")
    payload, _header = JWT.decode(token, SECRET, algorithm: "HS256")
    assert_equal "d", payload["sub"]
  end

  # The claims are UTF-8 text, written as bytes or as a JSON escape
  def test_multibyte_claims_read_as_text
    payload, _header = JWT.decode(TOKEN, SECRET, algorithm: "HS256")
    assert_equal 1, payload["raw"].size
    assert_equal 1, payload["esc"].size
    assert_equal payload["raw"], payload["esc"]
  end
end
