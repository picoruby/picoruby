# HID device test script for R2P2
require 'usb/hid'

puts "=== R2P2 HID Device Test ==="
puts ""

# Test 1: Keyboard
puts "Test 1: Keyboard (in 3 seconds...)"
puts "  Will type: 'Hello from R2P2!'"
sleep 3

USB::HID.send_text("Hello from R2P2!\n")
sleep 1

# Test 2: Mouse movement
puts "Test 2: Mouse movement"
puts "  Moving mouse in a square pattern..."
sleep 1

# Move right
USB::HID.move_mouse(50, 0, 0, 0)
sleep_ms(500)

# Move down
USB::HID.move_mouse(0, 50, 0, 0)
sleep_ms(500)

# Move left
USB::HID.move_mouse(-50, 0, 0, 0)
sleep_ms(500)

# Move up
USB::HID.move_mouse(0, -50, 0, 0)
sleep_ms(500)

# Test 3: Mouse click
puts "Test 3: Mouse click"
USB::HID.click_mouse(USB::HID::MOUSE_BTN_LEFT)
sleep 1

# Test 4: Media keys
puts "Test 4: Media control"
puts "  Sending volume up..."
USB::HID.send_media(USB::HID::MEDIA_VOLUME_UP)
sleep 1

puts ""
puts "=== All tests complete! ==="
