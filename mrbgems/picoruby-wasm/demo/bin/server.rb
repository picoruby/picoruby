#!/usr/bin/env ruby
require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'webrick'
end

ROOT = File.expand_path('../../www', __FILE__)
DIST = File.expand_path('../../../npm/dist', __FILE__)
RappROOT = File.expand_path('../../../../picoruby-wasm-rapp/demo', __FILE__)

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
  '/picoruby.js' => ['picoruby.js', 'application/javascript', DIST],
  '/init.iife.js' => ['init.iife.js', 'application/javascript', DIST],
  '/picoruby.wasm' => ['picoruby.wasm', 'application/wasm', DIST],
  '/rapp_demo.html' => ['rapp_demo.html', 'text/html', RappROOT],
  '/rapp_demo.rb' => ['rapp_demo.rb', 'text/ruby', RappROOT],
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

trap('INT') { server.shutdown }
server.start
