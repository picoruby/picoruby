# picoruby-shinonome

Shinonome font library for PicoRuby

## Overview

This gem provides Japanese font rendering capabilities for PicoRuby using the Shinonome font family. It converts BDF (Bitmap Distribution Format) font files into C header files that can be embedded in PicoRuby applications for terminal-based text rendering.

## Features

- **Japanese font support**: Full JIS X 0208 character set coverage
- **Multiple font sizes**: 12x12, 16x16 pixel fonts with various styles
- **ASCII font support**: 6x12 and 8x16 ASCII character sets
- **Build-time font conversion**: BDF files are converted to C headers during build
- **Memory efficient**: Bitmap fonts optimized for embedded systems
- **Unicode to JIS mapping**: Automatic conversion from Unicode to JIS encoding

## Supported Fonts

### ASCII Fonts
- **12**: 6x12 pixel ASCII characters (shnm6x12a.bdf)
- **16**: 8x16 pixel ASCII characters (shnm8x16a.bdf)

### Japanese Fonts (JIS X 0208)
- **12maru**: 12x12 pixel rounded style (shnmk12maru.bdf)
- **12go**: 12x12 pixel gothic style (shnmk12.bdf) 
- **12min**: 12x12 pixel mincho style (shnmk12min.bdf)
- **16go**: 16x16 pixel gothic style (shnmk16.bdf)
- **16min**: 16x16 pixel mincho style (shnmk16min.bdf)

## File Structure

```
mrbgems/picoruby-shinonome/
├── mrbgem.rake              # Gem specification and build configuration
├── mrblib/
│   └── shinonome.rb         # Ruby interface
├── src/
│   └── shinonome.c          # C implementation
├── include/
│   └── shinonome.h          # Header file
├── sig/
│   └── shinonome.rbs        # Type signatures
├── lib/
│   ├── JIS0208.TXT          # Unicode to JIS mapping table
│   └── shinonome-0.9.11/    # Original Shinonome font distribution
│       └── bdf/             # BDF font files
├── utils/
│   ├── font_convert.rb      # BDF to C header converter
│   └── unicode2jis.rb       # Unicode to JIS mapping generator
└── test/
    ├── Makefile
    └── test.c               # C test code
```

## Usage

### Basic Usage

```ruby
require 'shinonome'

# Render text with a specific font
Shinonome.draw(:go12, "Hello, 世界", 1) do |height, width, widths, glyphs|
  # height: font height in pixels
  # width: total width of the text
  # widths: array of character widths
  # glyphs: array of bitmap data for each character
  
  # Render the bitmap data
  height.times do |y|
    widths.each_with_index do |char_width, i|
      char_width.times do |x|
        bit = (glyphs[i][y] >> (char_width - 1 - x)) & 1
        print bit == 1 ? "██" : "  "
      end
    end
    puts
  end
end
```

### Available Font Methods

```ruby
# ASCII fonts
Shinonome.draw(:ascii12, text, scale)  # 6x12 ASCII
Shinonome.draw(:ascii16, text, scale)  # 8x16 ASCII

# Japanese fonts  
Shinonome.draw(:go12, text, scale)     # 12x12 gothic
Shinonome.draw(:go16, text, scale)     # 16x16 gothic
Shinonome.draw(:min12, text, scale)    # 12x12 mincho
Shinonome.draw(:min16, text, scale)    # 16x16 mincho
Shinonome.draw(:maru12, text, scale)   # 12x12 rounded
```

### Scaling

The scale parameter allows you to render fonts at different sizes:

```ruby
# Normal size
Shinonome.draw(:go12, "文字", 1)

# Double size
Shinonome.draw(:go12, "文字", 2)

# Triple size
Shinonome.draw(:go12, "文字", 3)
```

## Build Process

During the build process, the gem automatically:

1. **Converts BDF fonts to C headers**: Each BDF file is processed and converted to a C header file containing bitmap data
2. **Generates Unicode to JIS mapping**: Creates a lookup table for converting Unicode characters to JIS codes
3. **Compiles C extensions**: Links the font data with the C implementation

### Build Configuration

The build process is controlled by the `mrbgem.rake` file, which defines:
- Font file mappings (BDF source to C header destination)
- Font metadata (width, height, type)
- Build dependencies and include paths

## Character Encoding

- **Input**: Unicode (UTF-8) strings
- **Internal**: JIS X 0208 encoding for Japanese characters
- **ASCII**: Direct mapping for ASCII characters (0x20-0x7F)
- **Unicode to JIS**: Automatic conversion using JIS0208.TXT mapping table

## Technical Details

### Font Data Structure

Font bitmaps are stored as byte arrays where each bit represents a pixel:
- 1 bit = foreground pixel
- 0 bit = background pixel
- Bits are packed left-to-right, top-to-bottom

### Memory Usage

Font data is compiled into the binary, making it immediately available without file system access. Memory usage varies by font:
- ASCII 12: ~1KB
- ASCII 16: ~1.5KB  
- Japanese fonts: ~200-400KB (depending on character coverage)

## Dependencies

- Standard C library
- PicoRuby mruby implementation
- Shinonome font files (included)

## Original Shinonome Font

This gem includes the Shinonome font version 0.9.11, which is a high-quality Japanese bitmap font family. The original Shinonome project provides fonts optimized for screen display with excellent readability at small sizes.

## License

MIT

The Shinonome fonts are distributed under their original license (see lib/shinonome-0.9.11/LICENSE for details).
