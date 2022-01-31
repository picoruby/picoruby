[![C/C++ CI](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml)

## PicoRuby

PicoRuby is an alternative mruby implementation which is:

- Small foot print
  - ROM: 256KB
  - RAM: 128KB or less (depending on app code)
- Portable
  - Depends on only standard C library such as glibc, Newlib or Newlib-nano
- Reference microcontroller boards
  - Raspberry Pi Pico - Arm Cortex-M0+, 264 KB RAM, 2 MB ROM
  - PSoC5LP - Arm Cortex-M3, 128 KB RAM, 256 KB ROM

<img src="docs/logos/fukuokarubyaward.png" width="212">

### Set-up

- PicoRuby compiler
- [mruby/c (mrubyc/mrubyc)](https://github.com/mrubyc/mrubyc)
  - Another implementation of mruby virtual machine

### Build

- Prerequisites
  - C toolchain
  - git
  - ruby (should be CRuby)
- `git clone` this repository
- `rake` will build binaries for your machine
- ~~You can build library files for PSoC5LP by hitting `rake psoc5lp_lib`~~
  - ~~It requires you to have Docker though,~~
  - ~~You can make arm-none-eabi tools if you don't want to use Docker~~
  - ~~[hasumikin/cross_compilation_toolchains](https://github.com/hasumikin/cross_compilation_toolchains) may help you to make an environment~~
  - This feature is under reconstruction as of 2022

#### Cross compilation

You can do it like this:

```
CC=arm-linux-gnueabihf-gcc \
CFLAGS="-static -g -O3 -Wall -Wundef -Werror-implicit-function-declaration -Wwrite-strings" \
LDFLAGS="-static" \
rake
```

### Binaries

`rake` command will make three kinds of executable binary

- bin/picorbc
  - `bin/picorbc path/to/source.rb` makes `path/to/source.mrb` that is VM code runs on an mruby-compatible virtual machine
- bin/picoruby
  - `bin/picoruby source.rb` executes Ruby just like normal `ruby` command
  - You can also do like `bin/picoruby -e 'puts "Hello World!"'`
- bin/picoirb
  - A REPL implementation like irb and mirb

### Debug build and production build

`rake debug` command makes "debug build" which shows debug-print like this:

![](https://raw.githubusercontent.com/hasumikin/picoruby/master/docs/images/debug-print.png)

(Please replace `mmruby` with `picoruby` in mind. It's an old name)

A production build omits those debug-print.

### Presentations about PicoRuby

The author gave a talk about PRK Firmware in RubyConf 2021.
See the video on [YouTube](https://www.youtube.com/watch?v=SLSwn41iJX4&t=12s).

Japanese talks are available at
[RubyKaigi Takeout 2020](https://rubykaigi.org/2020-takeout/presentations/hasumikin.html)
and
[RubyKaigi Takeout 2021](https://rubykaigi.org/2021-takeout/presentations/hasumikin.html).

### Roadmap

PicoRuby is still developing halfway towards finishing as of 2021.

See implementation roadmap on [issue/6](https://github.com/hasumikin/picoruby/issues/6)

### Contributing to PicoRuby

Fork, fix, then send a pull request.

### Acknowledgement

Part of this project was coded by [Monstarlab](https://monstar-lab.com/) with the support of
the Ruby Association Grant Program
[2020](https://www.ruby.or.jp/en/news/20201022)
and
[2021](https://www.ruby.or.jp/en/news/20211025).

See also [picoruby/picoruby/wiki](https://github.com/picoruby/picoruby/wiki).

### License

Copyright © 2020-2022 HASUMI Hitoshi & Monstarlab. See MIT-LICENSE for further details.
