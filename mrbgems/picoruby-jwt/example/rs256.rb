require 'jwt'

puts "Start generating RSA key pair"
rsa_key = MbedTLS::PKey::RSA.new(2048)
private_key = rsa_key.to_pem
public_key = rsa_key.public_key.to_pem
puts "Private key:\n#{private_key}\n"
puts "Public key:\n#{public_key}\n"
rsa_key = nil # needs to be released

puts "=========================="

puts "Start sinning JWT token"
rsa_sign_key = MbedTLS::PKey::RSA.new(private_key)
jwt_header = { alg: 'RS256', typ: 'JWT' }
jwt_payload = { iss: 'my_app_name', sub: 'my_device_id', iat: Time.now.to_i, exp: Time.now.to_i + 3600 }
token = JWT.encode(jwt_payload, rsa_sign_key, 'RS256', jwt_header)
puts "JWT:\n#{token}\n"

puts "=========================="

puts "Start verifying JWT token"
rsa_verify_key = MbedTLS::PKey::RSA.new(public_key)
result = JWT.decode(token, rsa_verify_key, validate: true, algorithm: 'RS256')
puts "Result: #{result}"
