ENV = {}
ENV['TERM'] = "ansi" # may be overwritten by IO.wait_terminal
ENV['TZ'] = "JST-9"
ENV['PATH'] = "" # Should be set in Shell.setup_system_files
ENV['HOME'] = "" # Should be set in Shell.setup_system_files
ENV['WIFI_MODULE'] = ""
ENV['WIFI_CONFIG_PATH'] = "/etc/network/wifi.yml"
$LOAD_PATH = ["/lib"]
