# picoruby-io-console

Console/terminal I/O extensions for PicoRuby.

## Usage

```ruby
require 'io/console'

# Get single character without Enter
char = STDIN.getch
puts "You pressed: #{char}"

# Read without echo
STDIN.echo = false
password = STDIN.gets.chomp
STDIN.echo = true
puts "Password entered (hidden)"

# Raw mode (no line buffering, no echo)
STDIN.raw do |io|
  char = io.getc
  puts "Raw char: #{char.ord}"
end

# Cooked mode (normal terminal mode)
STDIN.cooked do |io|
  line = io.gets
end

# Check echo status
if STDIN.echo?
  puts "Echo is on"
end

# Clear screen
IO.clear_screen

# Get cursor position
row, col = IO.get_cursor_position
puts "Cursor at: #{row}, #{col}"

# Wait for terminal input with timeout
input = IO.wait_terminal(timeout: 5.0)  # 5 second timeout

# Non-blocking read
begin
  data = STDIN.read_nonblock(100)
  puts "Read: #{data}"
rescue IO::EAGAINWaitReadable
  puts "No data available"
end
```

## API

### IO Methods

- `getch()` - Read single character without echo
- `echo=(bool)` - Enable/disable echo
- `echo?()` - Check if echo is enabled
- `raw { }` - Execute block in raw mode
- `raw!()` - Enter raw mode
- `cooked { }` - Execute block in cooked (normal) mode
- `cooked!()` - Enter cooked mode
- `read_nonblock(maxlen)` - Non-blocking read

### Class Methods

- `IO.clear_screen()` - Clear terminal screen
- `IO.get_cursor_position()` - Get cursor position, returns `[row, col]`
- `IO.wait_terminal(timeout:)` - Wait for input with timeout

## Terminal Modes

### Raw Mode

- No line buffering (characters available immediately)
- No echo
- No special character processing
- Useful for interactive applications

### Cooked Mode

- Line buffering (wait for Enter)
- Echo enabled
- Normal terminal processing
- Default mode

## Notes

- Useful for interactive CLI applications
- Terminal editors, shells, games
- Password input (disable echo)
- Real-time key detection
