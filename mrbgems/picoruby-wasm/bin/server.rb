#!/usr/bin/env ruby

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'webrick'
end

server = WEBrick::HTTPServer.new(
  Port: 8080,
  DocumentRoot: File.expand_path('../../www', __FILE__)
)

server.config[:MimeTypes]['wasm'] = 'application/wasm'

server.mount_proc('/picoruby.js') do |req, res|
  res.body = File.read(File.expand_path('../../../../build/wasm/bin/picoruby.js', __FILE__))
  res['Content-Type'] = 'application/javascript'
end

server.mount_proc('/picoruby.wasm') do |req, res|
  accept_encoding = req['Accept-Encoding']
  if accept_encoding && accept_encoding.include?('br')
    res.body = File.read(File.expand_path('../../../../build/wasm/bin/picoruby.wasm.br', __FILE__))
    res['Content-Encoding'] = 'br'
  else
    res.body = File.read(File.expand_path('../../../../build/wasm/bin/picoruby.wasm.gz', __FILE__))
    res['Content-Encoding'] = 'gzip'
  end
  res['Content-Type'] = 'application/wasm'
  res['Vary'] = 'Accept-Encoding'
  res['Cache-Control'] = 'no-store, no-cache, must-revalidate'
  res['Pragma'] = 'no-cache'
  res['Expires'] = '0'
end

trap('INT') { server.shutdown }
server.start
