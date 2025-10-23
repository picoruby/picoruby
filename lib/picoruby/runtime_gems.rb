
class RuntimeGems
  include MRuby::LoadGems

  attr_reader :gems

  def initialize(&block)
    @gems =  MRuby::Gem::List.new
    self.gembox 'peripheral_utils'
  end

  def root
    MRUBY_ROOT
  end

  def self.export(output_dir:, compiler:)
    FileUtils.mkdir_p(output_dir)

    new.gems.each do |gem|
      puts "Processing gem: #{gem.name}"

      # Get Ruby source files from mrblib directory
      rb_files = Dir.glob("#{gem.dir}/mrblib/**/*.rb").sort

      if rb_files.empty?
        puts "  No Ruby files found in #{gem.name}, skipping..."
        next
      end

      # Create gem-specific directory
      gem_output_dir = "#{output_dir}/#{gem.name}"
      FileUtils.mkdir_p(gem_output_dir)

      # Compile each Ruby file to mrb
      rb_files.each do |rb_file|
        relative_path = rb_file.sub("#{gem.dir}/mrblib/", "")
        mrb_file = "#{gem_output_dir}/#{relative_path.sub(/\.rb$/, '.mrb')}"
        mrb_dir = File.dirname(mrb_file)
        FileUtils.mkdir_p(mrb_dir)

        puts "  Compiling #{relative_path}..."
        system("#{compiler} -o #{mrb_file} #{rb_file}") or raise "Compilation failed for #{rb_file}"
      end
    end

    puts "Runtime gems compiled to #{output_dir}"
  end
end
