[![C/C++ CI](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/picoruby/picoruby/actions/workflows/c-cpp.yml)

## PicoRuby

PicoRuby is an alternative mruby implementation which is:

- Small foot print
  - ROM: 256KB or less
  - RAM: 128KB or less
  - Reference board is PSoC5LP which has Arm 32bit processor, 256KB ROM and 64KB RAM
- Portable
  - Depends on only standard C library such as glibc, Newlib or Newlib-nano

### Set-up

- picorbc
  - pico mruby compiler. The main part of this repository
- [mruby/c (mrubyc/mrubyc)](https://github.com/mrubyc/mrubyc)
  - Another implementation of mruby virtual machine

### Build

- `git clone` this repository with `--recursive` option
- Then, just hitting `make` will build binaries for your machine
- You can build library files for PSoC5LP by hitting `make psoc5lp_lib`
  - It requires you to have Docker though,
  - You can make arm-none-eabi tools if you don't want to use Docker
  - [hasumikin/cross_compilation_toolchains](https://github.com/hasumikin/cross_compilation_toolchains) may help you to make an environment

### Binaries

`make` command will make three kinds of executable binary

- build/bin/host-production/alloc_libc/picorbc
  - `build/bin/host-production/alloc_libc/picorbc path/to/source.rb` makes `path/to/source.mrb` which is VM code runs on mruby VM
- build/bin/host-production/alloc_mrbc/picoruby and build/bin/host-debug/alloc_mrbc/picoruby
  - `build/bin/host-production/alloc_mrbc/picoruby source.rb` executes Ruby just like normal `ruby` command
  - You can also do it like `build/bin/host-production/alloc_mrbc/picoruby -e 'puts "Hello World!"'`
- build/bin/host-production/alloc_mrbc/picoirb and build/bin/host-debug/alloc_mrbc/picoirb
  - They are experimental REPL implementation

### Debug build and production build

`make` command makes "debug build" which shows debug-print like this:

![](https://raw.githubusercontent.com/hasumikin/picoruby/master/docs/images/debug-print.png)

(Please replace `mmruby` with `picoruby` in mind. It's an old name)

A production build omits debug-print.
You can get "production build with LIBC" like `build/bin/host-production/alloc_libc/picoruby` by `make host_production_libc`

### Presentation about ~~mmruby~~ PicoRuby

I gave a talk about this project on [RubyKaigi Takeout 2020](https://rubykaigi.org/2020-takeout).

- [Video](https://youtu.be/kDOf_tZKlLU)
- [Slide](https://slide.rabbit-shocker.org/authors/hasumikin/RubyKaigiTakeout2020/)

### Roadmap

PicoRuby is still developing halfway towards finishing as of 2021.

See implementation roadmap on [issue/6](https://github.com/hasumikin/picoruby/issues/6)

### Contributing to PicoRuby

Fork, fix, then send a pull request.

### Acknowledgement

Part of this project was coded by [Monstarlab](https://monstar-lab.com/) with the support of [the Ruby Association Grant Program 2020](https://www.ruby.or.jp/en/news/20201022).

### License

Copyright © 2020-2021 HASUMI Hitoshi & Monstarlab. See MIT-LICENSE for further details.
