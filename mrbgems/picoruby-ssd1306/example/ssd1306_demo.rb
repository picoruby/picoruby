require 'ssd1306'

width = 128
height = 64

i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 8, scl_pin: 9, frequency: 400_000)
display = SSD1306.new(i2c: i2c, w: width, h: height)

puts "Starting SSD1306 demo..."

puts "Test 1: Fill screen"
display.fill_screen(0xFF)
sleep 1
GC.start

puts "Test 2: Clear screen"
display.clear
sleep 1
GC.start

puts "Test 3: Drawing pixels"
display.clear
x = 0
while x < width
  y = 0
  while y < height
    display.set_pixel(x, y, 1)
    y += 8
  end
  x += 8
end
display.update_display
sleep 2
GC.start

puts "Test 4: Drawing lines"
display.clear
display.draw_line(0, 0, width-1, height-1, 1)
display.draw_line(width-1, 0, 0, height-1, 1)
display.draw_line(width/2, 0, width/2, height-1, 1)
display.draw_line(0, height/2, width-1, height/2, 1)
display.update_display
sleep 2
GC.start

puts "Test 5: Drawing rectangles"
display.clear
display.draw_rect(10, 10, 30, 20, 1, false)  # Outline
display.draw_rect(50, 20, 25, 15, 1, true)   # Filled
display.draw_rect(85, 35, 35, 25, 1, false)  # Another outline
display.update_display
sleep 2
GC.start

puts "Test 6: Bouncing rectangle animation using erase method"
display.clear

# Rectangle properties
rect_w = 12
rect_h = 8
rect_x = 10.0
rect_y = 10.0
velocity_x = 2.5
velocity_y = 1.8

# Animation loop
60.times do |frame|
  # Erase previous rectangle position
  display.erase(rect_x.to_i, rect_y.to_i, rect_w, rect_h) unless frame == 0
  # Update position
  rect_x += velocity_x
  rect_y += velocity_y
  # Bounce off edges
  if rect_x <= 0 || rect_x >= (width - rect_w)
    velocity_x = -velocity_x
    rect_x = rect_x <= 0 ? 0 : width - rect_w
  end
  if rect_y <= 0 || rect_y >= (height - rect_h)
    velocity_y = -velocity_y
    rect_y = rect_y <= 0 ? 0 : height - rect_h
  end

  # Draw rectangle at new position
  display.draw_rect(rect_x.to_i, rect_y.to_i, rect_w, rect_h, 1, true)
  display.update_display_optimized

  sleep 0.08  # ~12 FPS animation
  GC.start if frame % 10 == 0  # Periodic garbage collection
end

sleep 1
GC.start

puts "Test 7: Custom bitmap with draw_bytes (bytes method)"
display.clear
# Create a simple 16x14 smiley face pattern (string literal)
smiley_bytes = "\x00\x00\x3F\xFC\x7F\xFE\xF3\xCF\xF3\xCF\xFF\xFF\xFF\xFF\xFF\xFF\xE0\x07\xF0\x0F\xFC\x3F\x7F\xFE\x3F\xFC\x00\x00"
display.draw_bytes(x: 56, y: 25, w: 16, h: 14, data: smiley_bytes)
display.update_display_optimized
sleep 2
GC.start

puts "Test 8: Text display with shinonome font"
display.clear
display.draw_text(:terminus_6x12, 0, 0, "Hello PicoRubyWorld!!")
display.update_display_optimized
display.draw_text(:shinonome_min16, 0, 16, "黒暗森林", 2)
display.update_display_optimized
display.draw_text(:shinonome_maru12, 0, 52, "いろはにほへとちりぬる")
display.update_display_optimized
GC.start

puts "SSD1306 demo completed!"
