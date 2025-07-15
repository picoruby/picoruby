require_relative 'utils/font_convert.rb'
require_relative 'utils/unicode2jis.rb'

MRuby::Gem::Specification.new('picoruby-shinonome') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Shinonome font'

  include_dir = "#{build_dir}/include"
  cc.include_paths << include_dir
  directory include_dir

  SHINONOME_DIR = File.join(dir, "lib", "shinonome-0.9.11", "bdf")
  FONTS = [
    {name: "12",     w:  6, h: 12, src: "shnm6x12a.bdf",   dst: "ascii_12_table.h",     type: :ascii},
    {name: "16",     w:  8, h: 16, src: "shnm8x16a.bdf",   dst: "ascii_16_table.h",     type: :ascii},
    {name: "12maru", w: 12, h: 12, src: "shnmk12maru.bdf", dst: "jis208_12maru_table.h",type: :jis},
    {name: "12go",   w: 12, h: 12, src: "shnmk12.bdf",     dst: "jis208_12go_table.h",  type: :jis},
    {name: "12min",  w: 12, h: 12, src: "shnmk12min.bdf",  dst: "jis208_12min_table.h", type: :jis},
    {name: "16go",   w: 16, h: 16, src: "shnmk16.bdf",     dst: "jis208_16go_table.h",  type: :jis},
    {name: "16min",  w: 16, h: 16, src: "shnmk16min.bdf",  dst: "jis208_16min_table.h", type: :jis},
  ]
  FONTS.each do |font|
    font[:src] = "#{SHINONOME_DIR}/#{font[:src]}"
    font[:dst] = "#{include_dir}/#{font[:dst]}"
    file font[:dst] => [font[:src], include_dir] do
      FontConvert.make(font)
    end
    unless %w(clean deep_clean).include?(Rake.application.top_level_tasks.first)
      Rake::Task[font[:dst]].invoke
    end
  end


  unicode2jis = "#{include_dir}/unicode2jis.h"
  file unicode2jis => ["#{dir}/lib/JIS0208.TXT", include_dir] do |f|
    Unicode2JIS.make f.prerequisites.first, f.name
  end
  unless %w(clean deep_clean).include?(Rake.application.top_level_tasks.first)
    Rake::Task[unicode2jis].invoke
  end
end
