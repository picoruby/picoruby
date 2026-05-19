if !$*[0]
  puts "No input file specified"
elsif !$*[1]
  puts "No output file specified"
elsif !File.file?($*[0])
  puts "#{$*[0]}: No such file"
else
  script = ""
  File.open($*[0], "r") do |f|
    script = f.read(f.size).to_s # TODO: check size
  end
  begin
    mrb = PicoRubyVM::InstructionSequence.compile(script).to_binary
    File.open($*[1], "w") do |f|
      f.expand(mrb.length) if f.respond_to? :expand
      f.write(mrb)
    end
  rescue => e
    puts "#{e.message}"
  end
end
