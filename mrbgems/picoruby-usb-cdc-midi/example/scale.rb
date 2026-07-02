require "usb/cdc/midi"

output = USB::CDC::MIDIOutput.new
until output.connected?
  sleep_ms 50
end

note = 60
while note <= 72
  output.putevent(:note_on, 0, note, 100)
  sleep_ms 180
  output.putevent(:note_off, 0, note, 0)
  note += 1
end
