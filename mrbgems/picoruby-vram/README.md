# picoruby-vram

A high-performance VRAM (Video RAM) buffer management library for PicoRuby, designed for embedded displays and graphics applications.

## Overview

The `picoruby-vram` gem provides an efficient abstraction layer for managing display buffers in embedded systems. It supports paged memory organization, hardware-accelerated drawing operations, and dirty page tracking for optimal performance.

## Features

- **Page-based memory management**: Organize display memory into manageable pages
- **Multiple pixel formats**: Support for monochrome, grayscale, and RGB formats
- **Hardware-accelerated drawing**: Optimized pixel, line, and rectangle drawing operations
- **Dirty page tracking**: Only update changed display regions for better performance
- **Cross-VM compatibility**: Works with both mruby and mruby/c virtual machines

## Supported Pixel Formats

- `PIXEL_FORMAT_MONO`: 1-bit per pixel (monochrome)
- TODO:
  - `PIXEL_FORMAT_GRAY4`: 4-bit per pixel (16 grayscale levels)
  - `PIXEL_FORMAT_GRAY8`: 8-bit per pixel (256 grayscale levels)
  - `PIXEL_FORMAT_RGB565`: 16-bit per pixel (RGB color)
  - `PIXEL_FORMAT_RGB888`: 24-bit per pixel (full RGB color)
  - `PIXEL_FORMAT_ARGB8888`: 32-bit per pixel (RGB with alpha channel)

## Basic Usage

### Creating a VRAM Buffer

```ruby
# Create a 128x64 pixel display with 8 pages (1 column, 8 rows)
vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
vram.name = "SSD1306"
```

### Drawing Operations

```ruby
# Set individual pixels
vram.set_pixel(10, 20, 1)  # x=10, y=20, color=1

# Draw lines using Bresenham's algorithm
vram.draw_line(0, 0, 127, 63, 1)  # diagonal line

# Draw filled rectangles
vram.draw_rect(10, 10, 30, 20, 1)  # x, y, w, h, color
```

### Page Management

```ruby
# Get all pages (returns array of [col, row, data] tuples)
all_pages = vram.pages

# Get only dirty (modified) pages
dirty_pages = vram.dirty_pages

# Process pages for display update
dirty_pages.each do |col, row, data|
  # Send data to display hardware
  display_controller.update_page(row, data)
end
```

### Custom Display Drivers

The VRAM class can be adapted to various display controllers:

```ruby
# For displays with different page organizations
vram = VRAM.new(
  w: 320,      # Display width
  h: 240,      # Display height
  cols: 4,     # 4 columns of pages
  rows: 3      # 3 rows of pages
)
```

## Architecture

### Memory Layout

The VRAM class organizes display memory into pages for efficient management:

```
Display (128x64):
┌─────────────────┐
│     Page 0      │ ← Row 0 (y: 0-7)
├─────────────────┤
│     Page 1      │ ← Row 1 (y: 8-15)
├─────────────────┤
│     Page 2      │ ← Row 2 (y: 16-23)
├─────────────────┤
│       ...       │
└─────────────────┘
```

Each page contains a contiguous buffer of pixel data optimized for the target display format.

### Dirty Page Tracking

The library automatically tracks which pages have been modified:

1. Drawing operations mark affected pages as "dirty"
2. `dirty_pages` method returns only modified pages
3. After reading, dirty flags are automatically cleared
4. Enables incremental display updates for better performance

## Performance Considerations

- **Batch operations**: Group multiple drawing operations before calling `dirty_pages`
- **Page-aligned updates**: Design UI elements to align with page boundaries when possible
- **Minimal transfers**: Use `dirty_pages` instead of `pages` for display updates
- **Memory efficiency**: Choose appropriate pixel format for your display requirements

## Examples

See [../picoruby-ssd1306/example/ssd1306_demo.rb](../picoruby-ssd1306/example/ssd1306_demo.rb)
