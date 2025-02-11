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

[https://picoruby.github.io/](https://picoruby.github.io/)

<img src="docs/logos/fukuokarubyaward.png" width="212">

### Depends on

- [mruby/c (mrubyc/mrubyc)](https://github.com/mrubyc/mrubyc): Another implementation of mruby virtual machine

### Used by

- [PRK Firmware](https://github.com/picoruby/prk_firmware): Keyboard firmware for Raspberry Pi Pico

- [picoruby.wasm](https://www.npmjs.com/package/@picoruby/wasm-wasi): PicoRuby WASI runtime for WebAssembly

### Build

- Prerequisites
  - C toolchain
  - git
  - ruby (should be CRuby 3.0+)

```console
git clone --recursive https://github.com/picoruby/picoruby
cd picoruby/
rake
# PICORUBY_DEBUG=1 rake                         # for debug build
# PICORUBY_DEBUG=1 PICORUBY_NO_LIBC_ALLOC=1 rake  # for debug build using mruby/c's memory allocator
bin/picoruby -e 'puts "Hello World!"'
```

#### Cross compilation

See an example: [build_config/r2p2-cortex-m0plus.rb](https://github.com/picoruby/picoruby/blob/master/build_config/r2p2-cortex-m0plus.rb)

### Binaries

`rake` command will make three kinds of executable binary

- bin/picorbc
  - `bin/picorbc path/to/source.rb` makes `path/to/source.mrb` that is VM code runs on an mruby-compatible virtual machine
- bin/picoruby
  - `bin/picoruby source.rb` executes Ruby just like normal `ruby` command
  - You can do like `bin/picoruby path/to/your_script.rb` to run your script
- bin/r2p2
  - POSIX version of R2P2 (https://github.com/picoruby/R2P2)

### Roadmap

PicoRuby is still developing halfway towards finishing as of 2024.

See implementation roadmap on [issue/6](https://github.com/picoruby/picoruby/issues/6)

### Contributing to PicoRuby

Fork, patch, then send a pull request.

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

Copyright © 2020-2024 HASUMI Hitoshi. See MIT-LICENSE for further details.

Copyright © 2020-2021 Monstarlab. See MIT-LICENSE for further details.
