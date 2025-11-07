require_relative 'utils/terminus_font_convert.rb'

MRuby::Gem::Specification.new('picoruby-terminus') do |spec|
  spec.license = ['MIT', 'OFL-1.1']
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Terminus font'

  include_dir = "#{build_dir}/include"
  cc.include_paths << include_dir
  directory include_dir

  TERMINUS_DIR = File.join(dir, "lib", "terminus-font-4.49.1")
  TERMINUS_FONTS = [
    {name: "6x12",   w:  6, h: 12, src: "ter-u12n.bdf", dst: "terminus_6x12_table.h"},
    {name: "8x16",   w:  8, h: 16, src: "ter-u16n.bdf", dst: "terminus_8x16_table.h"},
    {name: "12x24",  w: 12, h: 24, src: "ter-u24n.bdf", dst: "terminus_12x24_table.h"},
    {name: "16x32",  w: 16, h: 32, src: "ter-u32n.bdf", dst: "terminus_16x32_table.h"},
  ]
  TERMINUS_FONTS.each do |font|
    font[:src] = "#{TERMINUS_DIR}/#{font[:src]}"
    font[:dst] = "#{include_dir}/#{font[:dst]}"
    file font[:dst] => [font[:src], include_dir] do
      TerminusFontConvert.make(font)
    end
    unless %w(clean deep_clean).include?(Rake.application.top_level_tasks.first)
      Rake::Task[font[:dst]].invoke
    end
  end
end
