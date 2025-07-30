require 'pitchdetector'

pd = PitchDetector.new(26)

note = PitchDetector::Note.new

pd.start

Signal.trap(:INT) do
  pd.stop
  puts "Pitch Detector stopped."
end

puts "Pitch Detector started. Press Ctrl-C to stop."

while true
  freq = pd.detect_pitch
  if freq
    puts note.freq_to_note(freq)
  end
  sleep_ms 10
end
