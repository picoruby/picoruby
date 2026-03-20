class ENVClass
  def each
    h = _hash
    keys = h.keys
    i = 0
    while i < keys.size
      key = keys[i]
      yield(key, h[key])
      i += 1
    end
  end
end

ENV = ENVClass.new

ENV_DEFAULT_WIFI_CONFIG_PATH = "/etc/network/wifi.yml"
ENV_DEFAULT_HOME = "/home"
