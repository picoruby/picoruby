#
# Base Encoding library for PicoRuby
# This provides Base45, Base58, and Base62 encoding/decoding.
#
# Author: Hitoshi HASUMI
# License: MIT
#

module BaseEncoding
  class EncodingError < StandardError; end
  class DecodeError < EncodingError; end
end

# Base58 encoding (Bitcoin/Flickr alphabets)
module Base58
  class Base58Error < BaseEncoding::EncodingError; end
  class DecodeError < Base58Error; end

  # Bitcoin alphabet (default) - excludes 0, O, I, l
  BITCOIN_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

  # Flickr alphabet - different order than Bitcoin
  FLICKR_ALPHABET = "123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ"

  ALPHABETS = {
    bitcoin: BITCOIN_ALPHABET,
    flickr: FLICKR_ALPHABET
  }

  def self.encode(data, alphabet: :bitcoin)
    return "" if data.nil? || data.empty?

    alphabet_str = ALPHABETS[alphabet] || BITCOIN_ALPHABET
    base = alphabet_str.length

    # Convert string to number
    num = 0
    data.each_byte do |byte|
      num = num * 256 + byte
    end

    # Handle zero specially
    if num == 0
      return alphabet_str[0]
    end

    # Convert to base58
    result = ""
    while num > 0
      remainder = num % base
      num = num / base
      result = alphabet_str[remainder] + result
    end

    # Add leading zeros (encoded as '1' in Bitcoin alphabet)
    leading_zeros = 0
    data.each_byte do |byte|
      break if byte != 0
      leading_zeros += 1
    end

    alphabet_str[0] * leading_zeros + result
  end

  def self.decode(encoded, alphabet: :bitcoin)
    return "" if encoded.nil? || encoded.empty?

    alphabet_str = ALPHABETS[alphabet] || BITCOIN_ALPHABET
    base = alphabet_str.length

    # Count leading zeros
    leading_zeros = 0
    encoded.each_char do |char|
      break if char != alphabet_str[0]
      leading_zeros += 1
    end

    # Convert from base58 to number
    num = 0
    encoded.each_char do |char|
      digit = alphabet_str.index(char)
      if digit.nil?
        raise DecodeError.new("Invalid character in Base58 string: #{char}")
      end
      num = num * base + digit
    end

    # Convert number to bytes
    result = ""
    while num > 0
      result = (num % 256).chr + result
      num = num / 256
    end

    # Add leading zero bytes
    ("\x00" * leading_zeros) + result
  end
end

# Base62 encoding (alphanumeric)
module Base62
  class Base62Error < BaseEncoding::EncodingError; end
  class DecodeError < Base62Error; end

  # 0-9, A-Z, a-z
  ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
  BASE = 62

  def self.encode(data)
    return "" if data.nil? || data.empty?

    # Convert string to number
    num = 0
    data.each_byte do |byte|
      num = num * 256 + byte
    end

    # Handle zero specially
    if num == 0
      return ALPHABET[0]
    end

    # Convert to base62
    result = ""
    while num > 0
      remainder = num % BASE
      num = num / BASE
      result = ALPHABET[remainder] + result
    end

    # Preserve leading zeros
    leading_zeros = 0
    data.each_byte do |byte|
      break if byte != 0
      leading_zeros += 1
    end

    ALPHABET[0] * leading_zeros + result
  end

  def self.decode(encoded)
    return "" if encoded.nil? || encoded.empty?

    # Count leading zeros
    leading_zeros = 0
    encoded.each_char do |char|
      break if char != ALPHABET[0]
      leading_zeros += 1
    end

    # Convert from base62 to number
    num = 0
    encoded.each_char do |char|
      digit = ALPHABET.index(char)
      if digit.nil?
        raise DecodeError.new("Invalid character in Base62 string: #{char}")
      end
      num = num * BASE + digit
    end

    # Convert number to bytes
    result = ""
    while num > 0
      result = (num % 256).chr + result
      num = num / 256
    end

    # Add leading zero bytes
    ("\x00" * leading_zeros) + result
  end
end

# Base45 encoding (QR code friendly)
module Base45
  class Base45Error < BaseEncoding::EncodingError; end
  class DecodeError < Base45Error; end

  # Base45 alphabet (QR code alphanumeric mode)
  ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:"
  BASE = 45

  def self.encode(data)
    return "" if data.nil? || data.empty?

    result = ""
    i = 0

    # Process pairs of bytes
    while i < data.length
      if i + 1 < data.length
        # Process two bytes
        value = data[i].ord * 256 + data[i + 1].ord

        # Encode as 3 characters
        c = value % BASE
        value = value / BASE
        d = value % BASE
        e = value / BASE

        result += ALPHABET[c]
        result += ALPHABET[d]
        result += ALPHABET[e]

        i += 2
      else
        # Process single byte
        value = data[i].ord

        # Encode as 2 characters
        c = value % BASE
        d = value / BASE

        result += ALPHABET[c]
        result += ALPHABET[d]

        i += 1
      end
    end

    result
  end

  def self.decode(encoded)
    return "" if encoded.nil? || encoded.empty?

    if encoded.length % 3 == 1
      raise DecodeError.new("Invalid Base45 string length")
    end

    result = ""
    i = 0

    while i < encoded.length
      if i + 2 < encoded.length
        # Decode 3 characters
        c = ALPHABET.index(encoded[i])
        d = ALPHABET.index(encoded[i + 1])
        e = ALPHABET.index(encoded[i + 2])

        if c.nil? || d.nil? || e.nil?
          raise DecodeError.new("Invalid character in Base45 string")
        end

        value = c + d * BASE + e * BASE * BASE

        if value > 65535
          raise DecodeError.new("Invalid Base45 value")
        end

        # Convert to 2 bytes
        result += (value / 256).chr
        result += (value % 256).chr

        i += 3
      else
        # Decode 2 characters (last pair if odd length)
        c = ALPHABET.index(encoded[i])
        d = ALPHABET.index(encoded[i + 1])

        if c.nil? || d.nil?
          raise DecodeError.new("Invalid character in Base45 string")
        end

        value = c + d * BASE

        if value > 255
          raise DecodeError.new("Invalid Base45 value")
        end

        result += value.chr

        i += 2
      end
    end

    result
  end
end
