# PicoRuby.wasm

PicoRuby.wasm is a PicoRuby port to WebAssembly.

## Usage

```html
<!DOCTYPE html>
<html>
  <head><meta charset="utf-8"></head>
  <body>
    <h1>DOM manipulation</h1>
    <button id="button">Click me!</button>
    <h2 id="container"></h2>
    <script type="text/ruby">
      require 'js'
      JS.global[:document].getElementById('button').addEventListener('click') do |_e|
        JS.global[:document].getElementById('container').innerText = 'Hello, PicoRuby!'
      end
    </script>
    <script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@latest/dist/init.iife.js"></script>
  </body>
</html>
```

You can also make an independent Ruby script:

```ruby
    <script type="text/ruby" src="your_script.rb"></script>
```

### Fetching

```ruby
response = JS.global.fetch('some.svg').await
if response[:status].to_poro == 200
  JS.global[:document].getElementById('container').innerHTML = response.to_binary
end
```

As of now, GET method is only supported.

### JS::Object#to_poro method

As of now, `JS::Object` class doesn't have methods like `to_i` and `to_s`.

Instead, `to_poro`[^1] method converts a JS object to a Ruby object, fallbacking to a JS::Object when not doable.

[^1]: *poro* stands for *Plain Old Ruby Object*.

Other than `JS::Object#to_poro`, you can use `JS::Object#to_binary` to get a binary data from an arrayBuffer.

### Multi tasks

```html
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
  </head>
  <body>
    <h1>Multi tasks</h1>
    <div>Open the console and see the output</div>
    <script type="text/ruby">
      while !(aho_task = Task.get('aho_task'))
        # Waiting for aho_task...
        sleep 0.1
      end
      i = 0
      while true
        i += 1
        if i % 3 == 0 || i.to_s.include?('3')
          aho_task.resume
        else
          puts "From main_task: #{i}"
        end
        sleep 1
      end
    </script>
    
    <script type="text/ruby">
      aho_task = Task.current
      aho_task.name = 'aho_task'
      aho_task.suspend
      while true
        puts "From aho_task: Aho!"
        aho_task.suspend
      end
    </script>

    <script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@latest/dist/init.iife.js"></script>
  </body>
</html>
```

## Ristriction due to the language design

Inside callbacks like `addEventListener`, you can't refer to variables outside the block:

```ruby
document = JS.global[:document]
document.getElementById('button').addEventListener('click') do |e|
  document.getElementById('container').innerText = 'Hello, PicoRuby!'
  #=> NameError: undefined local variable or method `document'
end
```

The restriction above is only for the JS callbacks.
You can refer to variables outside the block in general:

```ruby
lvar = 'Hello, PicoRuby!'
3.times do
  puts lvar
end
#=> Hello, PicoRuby! (3 times)
```

## Contributing

Fork [https://github.com/picoruby/picoruby](https://github.com/picoruby/picoruby), patch, and send a pull request.

### Build

```sh
git clone https://github.com/picoruby/picoruby
cd picoruby
MRUBY_CONFIG=wasm rake
```

Then, you can start a local server:

```sh
rake wasm_server
```

### Resources that you may want to dig into:

- `picoruby/mrbgems/picoruby-wasm/*`
- `picoruby/build_config/wasm.rb`

## License

MIT License

2024 (c) HASUMI Hitoshi a.k.a. @hasumikin
