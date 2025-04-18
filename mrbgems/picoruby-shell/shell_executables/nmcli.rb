# wifi_config.rb
#   To create a wifi configuration file

require "yaml"
require "picoline"
require "mbedtls"
require "base64"

encrypt_proc = Proc.new do |password|
  cipher = MbedTLS::Cipher.new("AES-256-CBC")
  cipher.encrypt
  key_len = cipher.key_len
  iv_len = cipher.iv_len
  unique_id = Machine.unique_id
  len = unique_id.length
  cipher.key = (unique_id * ((key_len / len + 1) * len))[0, key_len].to_s
  cipher.iv = (unique_id * ((iv_len / len + 1) * len))[0, iv_len].to_s
  ciphertext = cipher.update(password) + cipher.finish
  tag = cipher.write_tag
  tag + ciphertext
end

puts "Ctrd-D to exit"
cli = PicoLine.new
answers = {"wifi" => {}}

answers["country_code"] = cli.ask("Country Code?") do |q|
  q.default = "JP"
end
answers["wifi"]["ssid"] = cli.ask("WiFi SSID?")
password = cli.ask("WiFi Password? (leave blank if no password required)", allow_empty: true)
answers["wifi"]["encoded_password"] = if password.nil? || password.empty?
  nil
else
  Base64.encode64(encrypt_proc.call(password))
end
answers["wifi"]["auto_connect"] = cli.ask("Auto Connect? (y/n)") do |q|
  q.default = "y"
end == "y"
answers["wifi"]["retry_if_failed"] = cli.ask("Retry if failed? (y/n)") do |q|
  q.default = "n"
end == "y"
answers["wifi"]["watchdog"] = cli.ask("Use Watchdog? (y/n)") do |q|
  q.default = "n"
end == "y"

File.open(ENV['WIFI_CONFIG_PATH'], "w") do |f|
  f.write YAML.dump(answers)
end

puts
puts "Successfully saved to #{ENV['WIFI_CONFIG_PATH']}"
