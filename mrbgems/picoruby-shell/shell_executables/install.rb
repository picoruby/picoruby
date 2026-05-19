if !Shell::ARGV[0]
  puts "No input file specified"
elsif !Shell::ARGV[1]
  puts "No output file specified"
elsif !File.file?(Shell::ARGV[0])
  puts "#{Shell::ARGV[0]}: No such file"
else
  script = ""
  File.open(Shell::ARGV[0], "r") do |f|
    script = f.read(f.size).to_s # TODO: check size
  end
  begin
    mrb = PicoRubyVM::InstructionSequence.compile(script).to_binary
    File.open(Shell::ARGV[1], "w") do |f|
      f.expand(mrb.length) if f.respond_to? :expand
      f.write(mrb)
    end
  rescue => e
    puts "#{e.message}"
  end
end
