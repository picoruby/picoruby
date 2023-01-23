class Mouse

  KEYCODE = {
    KC_MS_BTN1:     0x301,
    KC_BTN1:        0x301,
    KC_MS_BTN2:     0x302,
    KC_BTN2:        0x302,
    KC_MS_BTN3:     0x303,
    KC_BTN3:        0x303,
    KC_MS_BTN4:     0x304,
    KC_BTN4:        0x304,
    KC_MS_BTN5:     0x305,
    KC_BTN5:        0x305,
    KC_MS_UP:       0x306,
    KC_MS_U:        0x306,
    KC_MS_DOWN:     0x307,
    KC_MS_D:        0x307,
    KC_MS_LEFT:     0x308,
    KC_MS_L:        0x308,
    KC_MS_RIGHT:    0x309,
    KC_MS_R:        0x309,
    KC_MS_WH_UP:    0x30a,
    KC_WH_U:        0x30a,
    KC_MS_WH_DOWN:  0x30b,
    KC_WH_D:        0x30b,
    KC_MS_WH_LEFT:  0x30c,
    KC_WH_L:        0x30c,
    KC_MS_WH_RIGHT: 0x30d,
    KC_WH_R:        0x30d
  }

  def initialize(params)
    @driver = params[:driver]
  end

  attr_accessor :task_proc
  attr_reader :driver

  def task(&block)
    @task_proc = block
  end
end
