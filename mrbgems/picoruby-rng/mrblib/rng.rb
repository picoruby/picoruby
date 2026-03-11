module RNG
  def self.uuid
    hex = ""
    i = 0
    while i < 32
      hex << (RNG.random_int % 16).to_s(16)
      i += 1
    end
    # Format UUID according to RFC 4122
    [ hex[0, 8],
      hex[8, 4],
      '4' + hex[13, 3].to_s,  # Version 4
      %w[8 9 a b][RNG.random_int % 4] + hex[17, 3].to_s,  # Variant
      hex[20, 12]
    ].join('-')
  end
end
