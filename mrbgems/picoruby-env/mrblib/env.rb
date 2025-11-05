class ENVClass
  def each
    _hash.each do |key, value|
      yield(key, value)
    end
  end
end

ENV = ENVClass.new

ENV_DEFAULT_WIFI_CONFIG_PATH = "/etc/network/wifi.yml"
ENV_DEFAULT_HOME = "/home"
