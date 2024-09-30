class Base64
  def self.urlsafe_encode64(string)
    Base64.encode64(string).tr('+/', '-_').tr('=', '')
  end

  def self.urlsafe_decode64(string)
    Base64.decode64(string.tr('-_', '+/').ljust((string.length + 3) & ~3, '='))
  end
end
