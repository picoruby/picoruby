class ENVClass

  def []=(key, value)
    if !key.is_a?(String) || !value.is_a?(String)
      raise TypeError, "Both key and value must be strings"
    end
    setenv(key, value)
  end

  def [](key)
    @env[key]
  end

end

ENV = ENVClass.new
