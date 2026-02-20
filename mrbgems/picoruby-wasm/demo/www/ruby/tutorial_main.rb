require 'js'

doc    = JS.document
output = doc.getElementById('output-box')

doc.getElementById('btn-tutorial').addEventListener('click') do |_e|
  output.innerText = 'Tutorial running - follow the steps in the PicoRuby panel...'

  # --- Pause 1: basics ---
  name     = "PicoRuby"
  count    = 42
  ratio    = 3.14
  langs    = ["Ruby", "C", "JavaScript"]
  settings = { debug: true, level: 3 }
  binding.irb

  # --- Pause 2, 3, 4: loop iteration ---
  results = []
  langs.each_with_index do |lang, i|
    upper = lang.upcase
    binding.irb
    results << "#{i}: #{upper}"
  end

  # --- Pause 5: step & next demo ---
  binding.irb
  total  = double(count)
  answer = "#{name} v#{total}"

  output.innerText = "Tutorial complete! answer=#{answer}, results=#{results.join(', ')}"
end
