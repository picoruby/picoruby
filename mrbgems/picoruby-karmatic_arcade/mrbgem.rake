require "pathname"
require_relative "../picoruby-bdffont/utils/bdffont_font_convert.rb"

module KarmaticArcadeFontConvert
  def self.make(dst_path, bdf_path)
    puts "Building #{dst_path}"
    font  = BDFFontConvert::PropBdfFont.new(bdf_path)
    table = BDFFontConvert::PropAsciiTableGenerator.new(font, "karmatic_arcade")
    File.write(dst_path, table.to_c)
  end
end

MRuby::Gem::Specification.new('picoruby-karmatic_arcade') do |spec|
  spec.license = ['MIT', 'FFC']
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'KarmaticArcade font'

  spec.add_dependency 'picoruby-bdffont'
  cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-bdffont/include"

  include_dir = "#{build_dir}/include"
  cc.include_paths << include_dir
  directory include_dir

  KARMATIC_ARCADE_BDF = File.join(dir, "lib", "KarmaticArcade-10.bdf")
  KARMATIC_ARCADE_DST = File.join(include_dir, "karmatic_arcade_10_table.h")

  file KARMATIC_ARCADE_DST => [KARMATIC_ARCADE_BDF, include_dir] do
    KarmaticArcadeFontConvert.make(KARMATIC_ARCADE_DST, KARMATIC_ARCADE_BDF)
  end

  tasks = Rake.application.top_level_tasks
  if (tasks & %w(default all femtoruby:debug femtoruby:prod picoruby:debug picoruby:prod)).any?
    Rake::Task[KARMATIC_ARCADE_DST].invoke
  end
end
