require 'pitchdetector'

pd = PitchDetector.new(26, volume_threshold: 50)

pd.start

Signal.trap(:INT) do
  puts "Pitch Detector stopping."
  pd.stop
  puts "Pitch Detector stopped."
end

puts "Pitch Detector started. Press Ctrl-C to stop."

while true
  freq = pd.detect_pitch
  if freq
    puts "Detected frequency: #{freq} Hz"
  end
  sleep_ms 200
end
