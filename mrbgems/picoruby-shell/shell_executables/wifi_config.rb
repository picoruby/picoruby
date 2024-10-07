# wifi_config.rb
#   To create a wifi configuration file

require "yaml"
require "picoline"

cli = PicoLine.new
answers = {"wifi" => {}}

answers["country_code"] = cli.ask("Country Code?") do |q|
  q.default = "JP"
end
answers["wifi"]["ssid"] = cli.ask("WiFi SSID?")
answers["wifi"]["password"] = cli.ask("WiFi Password?")
answers["auto_connect"] = cli.ask("Auto Connect? (y/n)") do |q|
  q.default = "y"
end == "y"

File.open(ENV['WIFI_CONFIG_PATH'], "w") do |f|
  f.write YAML.dump(answers)
end

puts
puts "Successfully saved to #{ENV['WIFI_CONFIG_PATH']}"
