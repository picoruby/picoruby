[![C/C++ CI](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml)

## PicoRuby

PicoRuby is an alternative mruby implementation which is:

- Small foot print (depending on build config)
  - RAM: 512 KB or less (PicoRuby with networking)
  - RAM: 128 KB or less (FemtoRuby)
- Portable
  - Depends on only standard C library such as glibc, Newlib or Newlib-nano, and emcc
- Reference microcontroller boards
  - Raspberry Pi Pico 2 W - Arm Cortex-M33, 520 KB RAM, 4 MB Flash with CYW43 network module
  - Raspberry Pi Pico - Arm Cortex-M0+, 264 KB RAM, 2 MB Flash

### FemtoRuby?

From version 4.x, we call the artifacts below:

- PicoRuby (used to be MicroRuby) ... mruby VM version
- FemtoRuby (used to be PicoRuby) ... mruby/c VM version

mruby VM version is mainly being developed.

### API documentation with some demo videos

[https://picoruby.org/](https://picoruby.org/)

<img src="docs/logos/fukuokarubyaward.png" width="212">

### Depends on

- [PicoRuby compiler (picoruby/mruby-compiler2)](https://github.com/picoruby/mruby-compiler2)
- [Estalloc (picoruby/estalloc)](https://github.com/picoruby/estalloc)
- [Littlefs (littlefs-project/littlefs)](https://github.com/littlefs-project/littlefs)
- [mruby (mruby/mruby)](https://github.com/mruby/mruby)
- [mruby/c (mrubyc/mrubyc)](https://github.com/mrubyc/mrubyc)

### Used by

- [R2P2](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-r2p2): PicoRuby shell system for Raspberry Pi Pico
- [R2P2-ESP32](https://github.com/picoruby/R2P2-ESP32): ESP32 port
- [PicoRuby.wasm](https://www.npmjs.com/package/@picoruby/wasm-wasi): PicoRuby WASI runtime for WebAssembly
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
rake                    # same for `rake picoruby:prod`
rake picoruby:debug     # for debug build

bin/picoruby -e 'puts "Hello World!"'

```
- Building on a mac
  - openssl is not linked in homebrew to avoid mixing with system ssl
  - extend the C/LD flags to point to the right locations:

```console
rake LDFLAGS=-L$(brew --prefix openssl@3)/lib CFLAGS=-I$(brew --prefix openssl@3)/include
```

#### FemtoRuby (mruby/c VM version)

```console
rake femtoruby:prod
# or
rake femtoruby:debug
```

#### Cross compilation

See an example: [build_config/r2p2-picoruby-pico.rb](build_config/r2p2-picoruby-pico.rb)

### Binaries

`rake` command will make three kinds of executable binary

- bin/picorbc
  - `bin/picorbc path/to/source.rb` makes `path/to/source.mrb` that is VM code runs on an mruby-compatible virtual machine
- bin/picoruby
  - `bin/picoruby source.rb` executes Ruby just like normal `ruby` command
  - `bin/picoruby -e "puts 'Hello1'"` also works
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
