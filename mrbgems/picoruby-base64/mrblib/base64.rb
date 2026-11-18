# The C side encodes and decodes (encode64 there has no line breaks and
# is also strict_encode64); this file adds CRuby's shape on top:
# encode64 breaks lines every 60 characters, strict_decode64 rejects what
# is not Base64, and the URL-safe pair takes the padding option.
class Base64
  # Base64 text with a newline after every 60 characters and at the end,
  # as CRuby's encode64 (RFC 2045) writes it
  def self.encode64(string)
    encoded = strict_encode64(string)
    return "" if encoded.empty?
    out = ""
    i = 0
    size = encoded.size
    while i < size
      out << encoded[i, 60].to_s << "\n"
      i += 60
    end
    out
  end

  # decode64 ignores characters outside the alphabet; this one raises
  # ArgumentError on them and on padding that does not fit
  def self.strict_decode64(string)
    size = string.bytesize
    raise ArgumentError, "invalid base64" if size % 4 != 0
    i = 0
    pad = 0
    while i < size
      b = string.getbyte(i).to_i
      if b == 61 # '='
        pad += 1
      elsif pad > 0 ||
            !((65 <= b && b <= 90) || (97 <= b && b <= 122) || (48 <= b && b <= 57) || b == 43 || b == 47)
        raise ArgumentError, "invalid base64"
      end
      i += 1
    end
    raise ArgumentError, "invalid base64" if pad > 2
    decode64(string)
  end

  def self.urlsafe_encode64(string, padding: true)
    encoded = strict_encode64(string).tr("+/", "-_")
    padding ? encoded : encoded.tr("=", "")
  end

  def self.urlsafe_decode64(string)
    converted = string.tr("-_", "+/")
    rem = converted.size % 4
    if rem != 0 && !converted.end_with?("=")
      converted = converted + "=" * (4 - rem)
    end
    strict_decode64(converted)
  end
end
