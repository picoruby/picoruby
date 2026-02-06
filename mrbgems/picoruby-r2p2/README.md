# R2P2

R2P2 (Ruby Rapid Portable Platform) is a shell system that runs on Raspberry Pi Pico, written in [PicoRuby](https://github.com/picoruby/picoruby).

This picogem (`picoruby-r2p2`) contains the firmware sources, cmake configuration, and build tasks to produce `.uf2` binaries for Raspberry Pi Pico boards.

## Supported boards and VMs

| Board     | Chip    | WiFi | picoruby (mruby/c VM) | microruby (mruby VM) |
|-----------|---------|------|-----------------------|----------------------|
| pico      | RP2040  | No   | Yes                   | -                    |
| pico_w    | RP2040  | Yes  | Yes                   | -                    |
| pico2     | RP2350  | No   | Yes                   | Yes                  |
| pico2_w   | RP2350  | Yes  | Yes                   | Yes                  |

## Usage

1. Download a release binary (`.uf2`) or build one from source (see below)
2. Hold the BOOTSEL button on your Pico and plug it into USB
3. Drag and drop the `.uf2` file into the `RPI-RP2` drive
4. Connect to the shell through a serial port with a terminal emulator, eg., in Linux:
   ```sh
   gtkterm --port /dev/ttyACM0
   ```

### About serial port terminal emulators

GTKTerm is strongly recommended for Linux and TeraTerm is the greatest for Windows.
Traditional CUI/TUI emulators such as cu, screen, and minicom often don't work well.

If you use macOS, try PuTTY, though the author has not confirmed it.
You may need to look for a fine configuration.

Most problems on the terminal emulator would come from CR/LF handling and escape sequence timing issue.

### Dual CDC USB ports for debug output

R2P2 uses dual CDC (Communications Device Class) USB ports to separate standard output and debug output:

- **CDC 0** (in Linux, typically `/dev/ttyACM0`): Main terminal for shell interaction and application stdout
- **CDC 1** (e.g. `/dev/ttyACM1`): Debug output (stderr) for system messages and debug prints

Debug output is only enabled in debug builds.

To view debug output:
```sh
# Terminal 1: Main shell
gtkterm --port /dev/ttyACM0

# Terminal 2: Debug messages
gtkterm --port /dev/ttyACM1
```

In Ruby code, use `Machine.debug_puts` to output to the debug port:
```ruby
Machine.debug_puts "Debug message"
```

## Build

### Prerequisites

- git
- Ruby (CRuby) and Bundler
- cmake (>= 3.22)
- arm-none-eabi-gcc
- arm-linux-gnueabihf-gcc
- qemu-arm-static

On macOS, install the ARM toolchain with:
```sh
brew install --cask gcc-arm-embedded
```

The author develops on WSL2-Ubuntu (x86-64 Windows host) and Ubuntu (x86-64 native).

### Setup

From the picoruby root directory:

```sh
bundle install
rake r2p2:setup
cd mrbgems/picoruby-r2p2/lib/pico-sdk && \
  git checkout 2.2.0 && \
  git submodule update --init --recursive && \
  cd -
cd mrbgems/picoruby-r2p2/lib/pico-extras && \
  git checkout sdk-2.2.0 && \
  git submodule update --init --recursive && \
  cd -
```

### Build commands

Build tasks follow the pattern `r2p2:{vm}:{board}:{mode}`:

```sh
# Debug build for Pico (RP2040, mruby/c VM)
rake r2p2:picoruby:pico:debug

# Production build for Pico W (RP2040, mruby/c VM, WiFi)
rake r2p2:picoruby:pico_w:prod

# Debug build for Pico 2 (RP2350, mruby/c VM)
rake r2p2:picoruby:pico2:debug

# Production build for Pico 2 W (RP2350, mruby/c VM, WiFi)
rake r2p2:picoruby:pico2_w:prod

# Debug build for Pico 2 (RP2350, mruby VM)
rake r2p2:microruby:pico2:debug

# Production build for Pico 2 W (RP2350, mruby VM, WiFi)
rake r2p2:microruby:pico2_w:prod
```

The output `.uf2` file is generated in:
```
build/r2p2/{vm}/{board}/{mode}/R2P2-{VM}-{BOARD}-{VERSION}-{DATE}-{REV}.uf2
```

### Clean

```sh
rake r2p2:picoruby:pico:clean
rake r2p2:microruby:pico2:clean
```

### Using an external pico-sdk

If you already have pico-sdk and pico-extras installed elsewhere, set the environment variables before building:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
rake r2p2:picoruby:pico:debug
```

### Debugging with Picoprobe

Debug builds produce an `.elf` file that can be debugged with Picoprobe and gdb-multiarch.

See the gist: https://gist.github.com/hasumikin/f508c092ced0b5d51779d472fbaf81e8

## Directory structure

```
picoruby-r2p2/
  mrbgem.rake              # Gem specification
  r2p2_config.rb           # PICO_SDK_TAG version constant
  README.md
  include/                 # C headers (tusb_config.h, io_rp2040.h)
  mrblib/                  # Ruby sources (main_task.rb)
  src/ports/r2p2/          # C sources (main.c, USB drivers, fault handler)
  cmake/
    CMakeLists.txt         # CMake build configuration
    pico_sdk_import.cmake
    pico_extras_import.cmake
  lib/
    pico-sdk/              # git submodule
    pico-extras/           # git submodule
    rp2040js/              # git submodule (emulator)
```

## Demonstration

### Opening crawl

<a href="https://youtu.be/JfN5BpTCYOw"><img src="https://raw.githubusercontent.com/picoruby/R2P2/master/doc/images/openingcralw.png" width="360" /></a>

### Tutorial short clips

#### Part 1

1. <a href="https://youtu.be/s4M4rBnPSus">Preparation</a> - Install R2P2 into Raspberry Pi Pico and open it in a terminal emulator.
2. <a href="https://youtu.be/ISU6TbIoIlQ">R2P2 shell</a> - Use shell commands to see a filesystem working on R2P2.
3. <a href="https://youtu.be/2ZKpOOjzKJc">Hello World!</a> - Run small Ruby scripts in IRB.

#### Part 2

1. <a href="https://youtu.be/qbs25xDu7t8">GPIO</a> - Blink the on-board LED using the GPIO class.
2. <a href="https://youtu.be/dPGCyQrX6Zg">ADC</a> - Measure the temperature using the on-chip ADC.

#### Part 3

1. <a href="https://youtu.be/PVkP_uNBOo0">IRB deeper</a> - Use the multi-line editor feature of IRB.
2. <a href="https://youtu.be/0uj4m4RI7lE">Time class</a> - Set the current time in the RTC of Pi Pico.
3. <a href="https://youtu.be/X1RdA6IE780">Text editor</a> - Create a Ruby script with the Vim-like text editor and execute it.
4. <a href="https://youtu.be/7nHNEUZnuKQ">Drag and Drop</a> - Drag and drop a Ruby script and execute it.
5. <a href="https://youtu.be/6_RomLChvYE">/home/app.rb</a> - Make Pi Pico an autostart device by writing /home/app.rb.

### Presentation

- Video (JA) from RubyWorld Conference 2022: [Link](https://youtu.be/rSBnpxzB4d8?t=11226)
- <a href="https://slide.rabbit-shocker.org/authors/hasumikin/RubyWorldConference2022/">Slide</a>

## License

MIT
