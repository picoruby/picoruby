require 'dfu'

path = ARGV[0]  # optional: destination path (skips A/B slot and meta)

reboot_required = false
begin
  updater = DFU::Updater.new(path: path)
  updater.receive(STDIN)
  if path
    puts "DFU: file saved to #{path}"
  else
    DFU.confirm
    puts "DFU: update complete."
    reboot_required = true
  end
rescue => e
  puts "DFU: error - #{e.message}"
end
if reboot_required
  Machine.reboot
end
