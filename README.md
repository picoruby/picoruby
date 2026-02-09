[![C/C++ CI](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml)

## PicoRuby

PicoRuby is an alternative mruby implementation which is:

- Small foot print
  - ROM: 256 KB (depending on build config)
  - RAM: 128 KB or less (depending on app code)
  - (Figures in 32 bit architecture)
- Portable
  - Depends on only standard C library such as glibc, Newlib or Newlib-nano
- Reference microcontroller boards
  - Raspberry Pi Pico - Arm Cortex-M0+, 264 KB RAM, 2 MB Flash

### API documentation with some demo videos

[https://picoruby.org/](https://picoruby.org/)

<img src="docs/logos/fukuokarubyaward.png" width="212">

### Depends on

- [mruby (mruby/mruby)](https://github.com/mruby/mruby)
- [mruby/c (mrubyc/mrubyc)](https://github.com/mrubyc/mrubyc): Another implementation of mruby virtual machine

### Used by

- [picoruby.wasm](https://www.npmjs.com/package/@picoruby/wasm-wasi): PicoRuby WASI runtime for WebAssembly
- [PRK Firmware](https://github.com/picoruby/prk_firmware): Keyboard firmware for Raspberry Pi Pico

## Testing

For detailed information on testing PicoRuby, refer to the [Testing Guide](docs/testing-guide.md).

### Build

- Prerequisites
  - C toolchain
  - git
  - ruby (should be CRuby 3.4+)

```console
git clone --recurse-submodules https://github.com/picoruby/picoruby
cd picoruby/
git submodule update --init --recursive # If you forgot --recurse-submodules when git clone
rake
# PICORUBY_DEBUG=1 rake                           # for debug build
# PICORUBY_DEBUG=1 PICORUBY_NO_LIBC_ALLOC=1 rake  # for debug build using mruby/c's memory allocator
bin/picoruby -e 'puts "Hello World!"'
```
- Building on a mac
  - openssl is not linked in homebrew to avoid mixing with system ssl
  - extend the C/LD flags to point to the right locations:
```
rake LDFLAGS=-L$(brew --prefix openssl@3)/lib CFLAGS=-I$(brew --prefix openssl@3)/include
```

#### Cross compilation

See an example: [build_config/r2p2-picoruby-pico.rb](build_config/r2p2-picoruby-pico.rb)

### Binaries

`rake` command will make three kinds of executable binary

- bin/picorbc
  - `bin/picorbc path/to/source.rb` makes `path/to/source.mrb` that is VM code runs on an mruby-compatible virtual machine
- bin/picoruby
  - `bin/picoruby source.rb` executes Ruby just like normal `ruby` command
  - You can do like `bin/picoruby path/to/your_script.rb` to run your script
- bin/r2p2
  - POSIX version of R2P2 (See [mrbgems/picoruby-r2p2](mrbgems/picoruby-r2p2) for the Raspi Pico edition)

### Acknowledgement

Part of this project was coded by [Monstarlab](https://monstar-lab.com/) with the support of
the Ruby Association Grant Program
[2020](https://www.ruby.or.jp/en/news/20201022)
and
[2021](https://www.ruby.or.jp/en/news/20211025).

See also [picoruby/picoruby/wiki](https://github.com/picoruby/picoruby/wiki).

### Stargazers over time
[![Stargazers over time](https://starchart.cc/picoruby/picoruby.svg?variant=adaptive)](https://starchart.cc/picoruby/picoruby)

### License

Copyright © 2020- HASUMI Hitoshi. See MIT-LICENSE for further details.

Copyright © 2020-2021 Monstarlab. See MIT-LICENSE for further details.
