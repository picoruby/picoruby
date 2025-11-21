#!/usr/bin/env ruby
require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'webrick'
end

# Accept 'picoruby' or 'microruby' as argument (default: picoruby)
variant = ARGV[0] || 'picoruby'
unless ['picoruby', 'microruby'].include?(variant)
  puts "Usage: #{$0} [picoruby|microruby]"
  exit 1
end

ROOT = File.expand_path('../../www', __FILE__)
DIST = File.expand_path("../../../npm-#{variant}/dist", __FILE__)
RubinoROOT = File.expand_path('../../../../picoruby-wasm-rubino/demo', __FILE__)

server = WEBrick::HTTPServer.new(
  Port: 8080,
  DocumentRoot: ROOT,
  ConfigureBlock: -> (server) {
    server[:HTTPHeader] = { 
      'Cache-Control' => 'no-store, no-cache, must-revalidate',
      'Pragma' => 'no-cache',
      'Expires' => '0'
    }
  }
)

server.config[:MimeTypes]['wasm'] = 'application/wasm'

js_filename = "#{variant}.js"
wasm_filename = "#{variant}.wasm"

ROUTES = {
  "/#{js_filename}" => [js_filename, 'application/javascript', DIST],
  '/init.iife.js' => ['init.iife.js', 'application/javascript', DIST],
  "/#{wasm_filename}" => [wasm_filename, 'application/wasm', DIST],
  '/rubino_demo.html' => ['rubino_demo.html', 'text/html', RubinoROOT],
  '/rubino_demo.rb' => ['rubino_demo.rb', 'text/ruby', RubinoROOT],
}

ROUTES.each do |path, (filename, content_type, dir)|
  server.mount_proc(path) do |req, res|
    res.body = File.read(File.join(dir, filename))
    res['Content-Type'] = content_type
    res['Cache-Control'] = 'no-store, no-cache, must-revalidate'
    res['Pragma'] = 'no-cache'
    res['Expires'] = '0'
  end
end

puts "Starting server for #{variant} on http://localhost:8080"
puts "Available routes:"
ROUTES.keys.each { |path| puts "  http://localhost:8080#{path}" }

trap('INT') { server.shutdown }
server.start
