#!/usr/bin/env ruby
require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'webrick'
end

ROOT = File.expand_path('../../www', __FILE__)
DIST = File.join(ROOT, '..', 'dist')

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

ROUTES = {
  '/picoruby.js' => ['picoruby.js', 'application/javascript'],
  '/init.iife.js' => ['init.iife.js', 'application/javascript'],
  '/picoruby.wasm' => ['picoruby.wasm', 'application/wasm'],
}

ROUTES.each do |path, (filename, content_type)|
  server.mount_proc(path) do |req, res|
    res.body = File.read(File.join(DIST, filename))
    res['Content-Type'] = content_type
    res['Cache-Control'] = 'no-store, no-cache, must-revalidate'
    res['Pragma'] = 'no-cache'
    res['Expires'] = '0'
  end
end

trap('INT') { server.shutdown }
server.start
