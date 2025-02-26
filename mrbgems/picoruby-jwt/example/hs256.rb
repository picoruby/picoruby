require 'jwt'

hmac_secret = "my$ecretK3"

puts "=========================="

puts "Start sinning JWT token"
jwt_header = { alg: 'HS256', typ: 'JWT' }
jwt_payload = { iss: 'my_app_name', sub: 'my_device_id', iat: Time.now.to_i, exp: Time.now.to_i + 3600 }
token = JWT.encode(jwt_payload, hmac_secret, 'HS256', jwt_header)
puts "JWT:\n#{token}\n"

puts "=========================="

puts "Start verifying JWT token"
result = JWT.decode(token, hmac_secret, validate: true, algorithm: 'HS256')
puts "Result: #{result}"

