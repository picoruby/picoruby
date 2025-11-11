# picoruby-prk-sounder

Piezo buzzer/speaker support for PRK Firmware - adds audio feedback to keyboards.

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Initialize sounder
kbd.init_sounder(pin: 20)

# Play tones
kbd.sounder.play_tone(440, 200)   # 440Hz (A4) for 200ms
kbd.sounder.play_tone(880, 100)   # 880Hz (A5) for 100ms

# Play melodies
kbd.sounder.play_melody([
  [440, 200],  # Note frequency (Hz), duration (ms)
  [494, 200],
  [523, 400]
])

# Control from keymap
kbd.add_layer :default, [
  %i(KC_Q KC_W SOUND_ON SOUND_OFF),
  # ...
]

kbd.start
```

## API

### Methods

- `play_tone(frequency, duration)` - Play single tone
  - `frequency`: Frequency in Hz
  - `duration`: Duration in milliseconds
- `play_melody(notes)` - Play sequence of notes
  - `notes`: Array of `[frequency, duration]` pairs
- `on()` / `off()` - Enable/disable sounder

## Common Frequencies

- C4: 262 Hz
- D4: 294 Hz
- E4: 330 Hz
- F4: 349 Hz
- G4: 392 Hz
- A4: 440 Hz
- B4: 494 Hz
- C5: 523 Hz

## Use Cases

- Key press feedback (click sounds)
- Layer change notifications
- Error/success alerts
- Startup/shutdown jingles
- Game sound effects

## Notes

- Requires PWM-capable GPIO pin
- Uses passive piezo buzzer (not active buzzer)
- Can be toggled on/off via keycodes or programmatically
- Keep durations short to avoid blocking keyboard operation
