# Snake Game Demo for PicoRuby official board (PRB)
# Hardware: OLED(SSD1306), Joystick(ADC), RotaryEncoder, Buttons(GPIO)
#           Piezo buzzers on GPIO10 (push-pull with GPIO11)

require 'ssd1306'
require 'adc'
require 'gpio'
require 'irq'
require 'rotary_encoder'
require 'pio'
require 'rng' # for Kernel#rand

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

GRID    = 4    # pixels per cell
COLS    = 32   # 128 / 4
ROWS    = 14   # 56 / 4  (64 - 8 status bar = 56)
FIELD_Y = 8    # top of play field (below status bar)

# Direction vectors [dx, dy]
DIR_UP    = [0, -1]
DIR_DOWN  = [0,  1]
DIR_LEFT  = [-1, 0]
DIR_RIGHT = [1,  0]

# Speed table (ms per tick) -- index 0..4
SPEEDS = [300, 225, 150, 100, 50]

# ---------------------------------------------------------------------------
# Helper methods
# ---------------------------------------------------------------------------

def spawn_food(snake)
  x = 0
  y = 0
  collides = true
  while collides
    x = rand(COLS)
    y = rand(ROWS)
    collides = false
    i = 0
    while i < snake.size
      seg = snake[i]
      if seg[0] == x && seg[1] == y
        collides = true
        break
      end
      i += 1
    end
  end
  [x, y]
end

def draw_cell(display, cx, cy, color)
  px = cx * GRID
  py = cy * GRID + FIELD_Y
  display.draw_rect(px, py, GRID, GRID, color, true)
end

def erase_cell(display, cx, cy)
  px = cx * GRID
  py = cy * GRID + FIELD_Y
  display.erase(px, py, GRID, GRID)
end

def draw_food(display, fx, fy)
  # Draw a smaller dot in center of cell
  px = fx * GRID + 1
  py = fy * GRID + FIELD_Y + 1
  display.draw_rect(px, py, 2, 2, 1, true)
end

def draw_status(display, score, speed_idx)
  # Clear status bar area
  display.erase(0, 0, 128, 8)
  text = "SC:#{score}  SPD:#{speed_idx + 1}"
  display.draw_text(:terminus_6x12, 0, 0, text)
end

def draw_title(display)
  display.clear
  display.draw_text(:terminus_6x12, 28, 10, "SNAKE GAME")
  display.draw_text(:terminus_6x12, 16, 30, "BTN A: START")
  display.draw_text(:terminus_6x12, 10, 45, "Encoder: Speed")
  display.update_display
end

def draw_game_over(display, score)
  display.clear
  display.draw_text(:terminus_6x12, 22, 10, "GAME  OVER")
  display.draw_text(:terminus_6x12, 28, 30, "SC:#{score}")
  display.draw_text(:terminus_6x12, 10, 48, "BTN A: RESTART")
  display.update_display
end

def draw_paused(display)
  # Overlay pause text in center
  display.erase(34, 24, 60, 16)
  display.draw_text(:terminus_6x12, 40, 26, "PAUSED")
  display.update_display
end

# ---------------------------------------------------------------------------
# PIO buzzer -- square wave on GPIO10/11 in push-pull
# ---------------------------------------------------------------------------
#
# PIO clock = 1 MHz (125 MHz / 125).
# Half-period counter X loaded from FIFO.
# Frequency ~= 1_000_000 / (2 * X).
# Write 0 to silence.

BUZZER_PIN_BASE = 10
BUZZER_PIO_FREQ = 1_000_000

tone_prog = PIO.asm do
  label :silence
  set :pins, 0b00                      # both pins low -- no sound
  pull                                 # blocking: wait until FIFO has data
  label :new_period
  mov :x, :osr                        # X = half-period counter
  wrap_target
  label :tone_loop
  set :pins, 0b11                      # both buzzers high
  mov :y, :x
  label :high_wait
  jmp :high_wait, cond: :y_dec
  set :pins, 0b00                      # both buzzers low
  mov :y, :x
  label :low_wait
  jmp :low_wait, cond: :y_dec
  pull block: false                    # non-blocking: check for new value
  mov :x, :osr
  jmp :silence, cond: :not_x      # X == 0 -> silence
  wrap                                 # implicit jump back to tone_loop
end

$buzzer_sm = PIO::StateMachine.new(
  pio: PIO::PIO0,
  sm: 0,
  program: tone_prog,
  freq: BUZZER_PIO_FREQ,
  set_pins: BUZZER_PIN_BASE,
  set_pin_count: 2
)
$buzzer_sm.start

# Sound queue: [[period, duration_ms], ...]
$sound_queue = []
$sound_timer = 0

def freq_to_period(freq)
  BUZZER_PIO_FREQ / (2 * freq)
end

def sound_tone(freq, ms)
  $sound_queue << [freq_to_period(freq), ms]
end

def sound_eat
  sound_tone(1500, 60)
end

def sound_start
  sound_tone(800, 80)
  sound_tone(1200, 120)
end

def sound_game_over
  sound_tone(400, 150)
  sound_tone(200, 300)
end

def sound_silence
  $sound_queue.clear
  $sound_timer = 0
  $buzzer_sm.put_nonblocking(0)
end

def sound_tick(elapsed)
  if 0 < $sound_timer
    $sound_timer -= elapsed
    if $sound_timer <= 0
      $sound_timer = 0
      if $sound_queue.empty?
        $buzzer_sm.put_nonblocking(0)
      end
    end
  end
  if $sound_timer <= 0 && !$sound_queue.empty?
    tone = $sound_queue[0]
    if $buzzer_sm.put_nonblocking(tone[0])
      $sound_queue.shift
      $sound_timer = tone[1]
    end
  end
end

# ---------------------------------------------------------------------------
# Hardware initialization
# ---------------------------------------------------------------------------

# OLED
i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 8, scl_pin: 9, frequency: 400_000)
display = SSD1306.new(i2c: i2c, w: 128, h: 64)
display.clear

# Joystick (ADC)
joy_x = ADC.new(27)
joy_y = ADC.new(26)

# Global variables for IRQ callbacks (mruby/c closure limitation)
$btn_flags = { start: false, pause: false }
$speed_delta = 0

# Buttons (GPIO + IRQ)
btn_start = GPIO.new(17, GPIO::IN | GPIO::PULL_UP)
btn_pause = GPIO.new(18, GPIO::IN | GPIO::PULL_UP)

btn_start.irq(GPIO::EDGE_FALL, debounce: 200) do |_gpio, _event|
  $btn_flags[:start] = true
end

btn_pause.irq(GPIO::EDGE_FALL, debounce: 200) do |_gpio, _event|
  $btn_flags[:pause] = true
end

# Rotary Encoder
encoder = RotaryEncoder.new(20, 21)
encoder.cw  { $speed_delta += 1 }
encoder.ccw { $speed_delta -= 1 }

# ---------------------------------------------------------------------------
# Game logic helpers
# ---------------------------------------------------------------------------

def reset_game
  snake = []
  cx = COLS / 2
  cy = ROWS / 2
  # Initial snake length 3, going right
  i = 0
  while i < 3
    snake.unshift([cx - i, cy])
    i += 1
  end
  food = spawn_food(snake)
  [snake, DIR_RIGHT, DIR_RIGHT, food[0], food[1], 0]
end

def opposite?(d1, d2)
  d1[0] + d2[0] == 0 && d1[1] + d2[1] == 0
end

JOY_CENTER = 1850
JOY_DEAD   = 200

def read_joystick(joy_x_adc, joy_y_adc)
  raw_x = joy_x_adc.read_raw
  raw_y = joy_y_adc.read_raw
  dx = raw_x - JOY_CENTER
  dy = raw_y - JOY_CENTER

  # Determine which axis has larger deflection
  abs_dx = dx
  abs_dx = -abs_dx if dx < 0
  abs_dy = dy
  abs_dy = -abs_dy if dy < 0

  if abs_dx < JOY_DEAD && abs_dy < JOY_DEAD
    return nil
  end

  if abs_dy <= abs_dx
    # Horizontal dominant
    if 0 < dx
      DIR_RIGHT
    else
      DIR_LEFT
    end
  else
    # Vertical dominant
    if 0 < dy
      DIR_UP
    else
      DIR_DOWN
    end
  end
end

def full_redraw(display, snake, food_x, food_y, score, speed_idx)
  display.clear
  draw_status(display, score, speed_idx)
  # Draw snake
  i = 0
  while i < snake.size
    draw_cell(display, snake[i][0], snake[i][1], 1)
    i += 1
  end
  # Draw food
  draw_food(display, food_x, food_y)
  display.update_display
end

# ---------------------------------------------------------------------------
# Game state
# ---------------------------------------------------------------------------

state     = :title
snake     = [[COLS / 2, ROWS / 2]]
dir       = DIR_RIGHT
next_dir  = DIR_RIGHT
food_x    = 0
food_y    = 0
score     = 0
speed_idx = 2        # default speed level (0..4)
tick_acc  = 0        # accumulated ms for game tick
gc_count  = 0        # loop counter for periodic GC
needs_full_redraw = false

# Show title screen
draw_title(display)

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

last_ms = Machine.uptime_us / 1000

while true
  now_ms = Machine.uptime_us / 1000
  elapsed = now_ms - last_ms
  # Handle wrap-around
  if elapsed < 0
    elapsed = 10
  end
  last_ms = now_ms

  IRQ.process
  sound_tick(elapsed)

  # Process speed encoder
  if $speed_delta != 0
    speed_idx += $speed_delta
    $speed_delta = 0
    speed_idx = 0 if speed_idx < 0
    speed_idx = 4 if 4 < speed_idx
    if state == :playing || state == :paused
      draw_status(display, score, speed_idx)
      display.update_display_optimized
    end
  end

  # State machine
  case state
  when :title
    if $btn_flags[:start]
      $btn_flags[:start] = false
      snake, dir, next_dir, food_x, food_y, score = reset_game
      state = :playing
      needs_full_redraw = true
      tick_acc = 0
      sound_start
    end

  when :playing
    if $btn_flags[:pause]
      $btn_flags[:pause] = false
      state = :paused
      draw_paused(display)
    end
    $btn_flags[:start] = false  # ignore start during play

    # Read joystick
    joy_dir = read_joystick(joy_x, joy_y)
    if joy_dir && !opposite?(joy_dir, dir)
      next_dir = joy_dir
    end

    # Game tick
    tick_acc += elapsed
    if SPEEDS[speed_idx] <= tick_acc
      tick_acc = 0

      # Move snake
      head = snake[snake.size - 1]
      new_x = head[0] + next_dir[0]
      new_y = head[1] + next_dir[1]
      dir = next_dir

      # Wall collision
      if new_x < 0 || COLS <= new_x || new_y < 0 || ROWS <= new_y
        state = :game_over
        sound_game_over
        draw_game_over(display, score)
      else
        # Self collision
        hit_self = false
        i = 0
        while i < snake.size
          if snake[i][0] == new_x && snake[i][1] == new_y
            hit_self = true
            break
          end
          i += 1
        end

        if hit_self
          state = :game_over
          sound_game_over
          draw_game_over(display, score)
        else
          # Advance snake
          snake.push([new_x, new_y])

          if new_x == food_x && new_y == food_y
            # Ate food
            sound_eat
            score += 1
            food = spawn_food(snake)
            food_x = food[0]
            food_y = food[1]
            needs_full_redraw = true
          else
            # Remove tail
            tail = snake.shift
            erase_cell(display, tail[0], tail[1])
          end

          if needs_full_redraw
            full_redraw(display, snake, food_x, food_y, score, speed_idx)
            needs_full_redraw = false
          else
            # Incremental draw: new head + food (food might overlap erase)
            draw_cell(display, new_x, new_y, 1)
            draw_food(display, food_x, food_y)
            draw_status(display, score, speed_idx)
            display.update_display_optimized
          end
        end
      end
    end

  when :paused
    if $btn_flags[:pause]
      $btn_flags[:pause] = false
      state = :playing
      tick_acc = 0
      needs_full_redraw = true
    end
    if $btn_flags[:start]
      $btn_flags[:start] = false
    end
    # Redraw field on resume
    if needs_full_redraw
      full_redraw(display, snake, food_x, food_y, score, speed_idx)
      needs_full_redraw = false
    end

  when :game_over
    if $btn_flags[:start]
      $btn_flags[:start] = false
      state = :title
      draw_title(display)
    end
    $btn_flags[:pause] = false
  end

  # Periodic GC
  gc_count += 1
  if 20 <= gc_count
    gc_count = 0
    GC.start
  end

  sleep_ms 10
end
