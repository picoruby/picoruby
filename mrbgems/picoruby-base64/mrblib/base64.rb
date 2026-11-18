# The C side has encode64, strict_encode64, decode64 and strict_decode64;
# this file adds the URL-safe pair, with CRuby's padding option.
class Base64
  def self.urlsafe_encode64(string, padding: true)
    encoded = strict_encode64(string).tr("+/", "-_")
    padding ? encoded : encoded.tr("=", "")
  end

  # Padding may be left out; anything else that is not Base64 raises
  # ArgumentError, as with CRuby
  def self.urlsafe_decode64(string)
    converted = string.tr("-_", "+/")
    rem = converted.size % 4
    if rem != 0 && !converted.end_with?("=")
      converted = converted + "=" * (4 - rem)
    end
    strict_decode64(converted)
  end
end
