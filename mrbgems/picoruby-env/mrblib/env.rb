class ENVClass

  def []=(key, value)
    if !key.is_a?(String) || !value.is_a?(String)
      raise TypeError, "Both key and value must be strings"
    end
    setenv(key, value)
    @env[key] = value
  end

  def [](key)
    @env[key]
  end

end

ENV = ENVClass.new
ENV["PWD"]  ||= ""      # Should be set in Shell.setup_system_files
ENV["TERM"] ||= "ansi"  # may be overwritten by IO.wait_terminal
ENV["TZ"]   ||= "JST-9" # TODO. maybe in CYW43
ENV["WIFI_MODULE"] ||= "none" # possibly overwritten in CYW43.init
