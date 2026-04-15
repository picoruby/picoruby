require "spi"
require "gpio"
require "vram"
require "bdffont"

class UC8151
  include VRAM::Delegatable

  BDFFont.setup(self)

  # UC8151 command registers
  PSR  = 0x00  # Panel Setting
  PWR  = 0x01  # Power Setting
  POF  = 0x02  # Power OFF
  POFS = 0x03  # Power OFF Sequence Setting
  PON  = 0x04  # Power ON
  BTST = 0x06  # Booster Soft Start
  DSLP = 0x07  # Deep Sleep
  DTM1 = 0x10  # Data Transmission 1 (old/background data)
  DSP  = 0x11  # Data Stop
  DRF  = 0x12  # Display Refresh
  DTM2 = 0x13  # Data Transmission 2 (new data)
  PLL  = 0x30  # PLL Control
  CDI  = 0x50  # VCOM and Data Interval Setting
  TCON = 0x60  # Gate/Source Non-overlap Period
  TRES = 0x61  # Resolution Setting

  def initialize(spi:, cs_pin:, dc_pin:, rst_pin:, busy_pin:, w: 296, h: 128)
    @spi  = spi
    @cs   = GPIO.new(cs_pin,   GPIO::OUT)
    @cs.write(1)
    @dc   = GPIO.new(dc_pin,   GPIO::OUT)
    @rst  = GPIO.new(rst_pin,  GPIO::OUT)
    @busy = GPIO.new(busy_pin, GPIO::IN)
    @width  = w
    @height = h
    @frame_size = @width * @height / 8
    # rotate: 90 maps landscape (x=0..295, y=0..127) to the column-major
    # buffer format the UC8151 expects: (buf_x=y, buf_y=295-x).
    # invert: true because bit=1 means white (paper) in KW mode.
    @vram = VRAM.new(w: @width, h: @height, cols: 1, rows: 1,
                     layout: :horizontal, invert: true, rotate: 90)
    @vram.name = "UC8151"
    reset
    init_display
    deep_clean
  end

  private def reset
    @rst.write(1)
    sleep_ms 100
    @rst.write(0)
    sleep_ms 100
    @rst.write(1)
    sleep_ms 100
  end

  # UC8151 BUSY pin: LOW = busy, HIGH = idle
  private def busy_wait
    sleep_ms 10
    while @busy.read == 0
      sleep_ms 10
    end
  end

  # Send command byte in its own CS transaction (DC=0).
  private def command(cmd)
    @dc.write(0)
    @cs.write(0)
    @spi.write(cmd)
    @cs.write(1)
  end

  # Send data bytes in their own CS transaction (DC=1).
  private def send_data(*bytes)
    @dc.write(1)
    @cs.write(0)
    i = 0
    while i < bytes.size
      @spi.write(bytes[i])
      i += 1
    end
    @cs.write(1)
  end

  # Send a data string in 1024-byte chunks in one CS transaction (DC=1).
  private def send_data_chunked(data)
    @dc.write(1)
    @cs.write(0)
    i = 0
    while i < data.size
      chunk_size = (data.size - i) < 1024 ? (data.size - i) : 1024
      @spi.write(data[i, chunk_size]) # steep:ignore
      i += 1024
    end
    @cs.write(1)
  end

  private def init_display
    # PSR: 128x296, KW mode (black/white), booster on, soft reset off
    command(PSR); send_data(0x5f)
    # PWR: VDS/VDG internal, VCOM internal, VGH=20V, VGL=-20V, VDH=15V, VDL=-15V
    #      5th byte is VCOM voltage: raised from 0x17 to 0x1e for stronger drive
    command(PWR); send_data(0x03, 0x00, 0x2b, 0x2b, 0x1e)
    # BTST: booster soft start phases A/B/C strength and minimum OFF time
    command(BTST); send_data(0x17, 0x17, 0x17)
    # PLL: 50Hz frame rate (must be sent before PON)
    command(PLL); send_data(0x3c)
    # PON: power on, then wait for panel to become ready
    command(PON)
    busy_wait
    # TRES: gate resolution (128 = 0x80) first, then source resolution (296 = 0x0128)
    command(TRES); send_data(0x80, 0x01, 0x28)
    # CDI: border control and VCOM data interval
    #      0x13 optimizes border handling and pixel polarity
    command(CDI); send_data(0x13)
    # TCON: gate/source non-overlap periods
    command(TCON); send_data(0x22)
  end

  # Reset the panel memory before first use to avoid ghost images.
  private def deep_clean
    command(DTM1)
    send_data_chunked("\x00" * @frame_size)
    command(DTM2)
    send_data_chunked("\xFF" * @frame_size)
    command(DRF)
    busy_wait
  end

  # Send VRAM contents to display and trigger refresh.
  def update
    pages = @vram.pages
    # DTM1 carries the previous frame (all-white = no ghost)
    command(DTM1)
    send_data_chunked("\xFF" * @frame_size)
    # DTM2 carries the new frame
    command(DTM2)
    send_data_chunked(pages[0][2])
    command(DRF)
    busy_wait
    command(POF)
  end

  def fill(color = 0)
    @vram.fill(color)
  end
end
