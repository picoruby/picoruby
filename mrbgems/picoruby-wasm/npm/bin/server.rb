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
  DocumentRoot: ROOT
)

server.config[:MimeTypes]['wasm'] = 'application/wasm'

ROUTES = {
  '/picoruby.js' => ['picoruby.js', 'application/javascript'],
  '/picoruby.iife.js' => ['picoruby.iife.js', 'application/javascript'],
  '/picoruby.wasm' => ['picoruby.wasm', 'application/wasm']
}

CACHE_HEADERS = {
  'Cache-Control' => 'no-store, no-cache, must-revalidate',
  'Pragma' => 'no-cache',
  'Expires' => '0'
}

ROUTES.each do |path, (filename, content_type)|
  server.mount_proc(path) do |req, res|
    res.body = File.read(File.join(DIST, filename))
    res['Content-Type'] = content_type
    CACHE_HEADERS.each { |key, value| res[key] = value }
  end
end

trap('INT') { server.shutdown }
server.start
