# picoruby-uri

Simplified, CRuby-compatible `URI` utilities for PicoRuby. Pure Ruby, so it
runs on microcontroller targets and on picoruby.wasm alike.

Extracted from picoruby-net-http (which now depends on this gem).

## API

```ruby
require 'uri'

uri = URI.parse("https://example.com:8443/search?q=ruby#top")
uri.scheme      # => "https"
uri.host        # => "example.com"
uri.port        # => 8443
uri.path        # => "/search"
uri.query       # => "q=ruby"
uri.fragment    # => "top"
uri.request_uri # => "/search?q=ruby"

URI.encode_www_form_component("hello world") # => "hello+world"
URI.encode_www_form(page: 2, q: "hello world")
# => "page=2&q=hello+world"
```

- Only `http` / `https` URIs are supported by `URI.parse`.
- `encode_www_form_component` follows CRuby: bytes outside `[A-Za-z0-9*-._]`
  are percent-encoded byte-wise (multibyte UTF-8 works), a space becomes
  `+`, and the tilde is encoded (`%7E`).
- `encode_www_form` accepts a Hash or an Array of `[key, value]` pairs. Array
  values are expanded as repeated keys, and a `nil` value is emitted without
  an equals sign, matching CRuby.
