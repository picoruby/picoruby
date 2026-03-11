require 'drb'

class MyService
  def hello1
    "Hello"
  end
  def hello2(name)
    "Hello, #{name}!"
  end
end

SERVER_URI = 'ws://0.0.0.0:9090'
DRb.start_service(SERVER_URI, MyService.new)
DRb.thread.join

# From client side:
# require 'drb'
# service = DRb::DRbObject.new_with_uri(uri)
# puts service.hello1 # => "Hello"
# puts service.hello2('World') # => "Hello, World!"
