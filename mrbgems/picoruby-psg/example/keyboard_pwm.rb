require 'psg'

channel = 2
driver = PSG::Driver.new(:pwm, left: 10, right: 11)

driver.set_timbre(channel, PSG::Driver::TIMBRES[:square], 0)
driver.set_pan(channel, 8, 0)
driver.send_reg(channel + 8, 12, 0)
driver.mute(channel, 1, 0)

keyboard = PSG::Keyboard.new(driver, channel: channel)

begin
  keyboard.start
ensure
  keyboard.note_off
  driver.join
end
