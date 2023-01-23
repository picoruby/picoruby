class Mouse

  KEYCODE = {
    KC_MS_BTN1:     0x301, # 0b00001
    KC_BTN1:        0x301,
    KC_MS_BTN2:     0x302, # 0b00010
    KC_BTN2:        0x302,
    KC_MS_BTN3:     0x304, # 0b00100
    KC_BTN3:        0x304,
    KC_MS_BTN4:     0x308, # 0b01000
    KC_BTN4:        0x308,
    KC_MS_BTN5:     0x310, # 0b10000
    KC_BTN5:        0x310,
    KC_MS_UP:       0x311,
    KC_MS_U:        0x311,
    KC_MS_DOWN:     0x312,
    KC_MS_D:        0x312,
    KC_MS_LEFT:     0x313,
    KC_MS_L:        0x313,
    KC_MS_RIGHT:    0x314,
    KC_MS_R:        0x314,
    KC_MS_WH_UP:    0x315,
    KC_WH_U:        0x315,
    KC_MS_WH_DOWN:  0x316,
    KC_WH_D:        0x316,
    KC_MS_WH_LEFT:  0x317,
    KC_WH_L:        0x317,
    KC_MS_WH_RIGHT: 0x318,
    KC_WH_R:        0x318
  }

  def initialize(params)
    @driver = params[:driver]
    @cursor_speed = 5
    @wheel_speed = 1
  end

  attr_accessor :cursor_speed, :wheel_speed
  attr_accessor :task_proc
  attr_reader :driver

  def task(&block)
    @task_proc = block
  end
end
