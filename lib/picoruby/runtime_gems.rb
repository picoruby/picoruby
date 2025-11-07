
class RuntimeGems
  include Rake::DSL
  include MRuby::LoadGems

  attr_reader :gems

  def initialize(&block)
    @gems =  MRuby::Gem::List.new
    self.gembox 'peripheral_utils_1'
  end

  def root
    MRUBY_ROOT
  end

  def self.export(output_dir:)
    new.export(
      output_dir: output_dir,
    )
  end

  def export(output_dir:)
    mrb_files = []
    FileUtils.mkdir_p(output_dir)

    # Check all gems for C source files
    gems.each do |gem|
      c_files = Dir.glob("#{gem.dir}/src/**/*.{c,cpp,cc,cxx,h,hpp}")
      unless c_files.empty?
        raise "Error: Gem '#{gem.name}' contains C source files, which are not supported for runtime gems.\n" \
              "C files found: #{c_files.map { |f| f.sub("#{gem.dir}/", "") }.join(', ')}"
      end
    end

    # Compile Ruby files to mrb
    gems.each do |gem|
      # Get Ruby source files from mrblib directory
      rb_files = Dir.glob("#{gem.dir}/mrblib/**/*.rb")

      # Create gem-specific directory
      gem_output_dir = "#{output_dir}/#{gem.name}"
      FileUtils.mkdir_p(gem_output_dir)

      # Compile each Ruby file to mrb
      rb_files.each do |rb_file|
        relative_path = rb_file.sub("#{gem.dir}/mrblib/", "")
        mrb_file = "#{gem_output_dir}/#{relative_path.sub(/\.rb$/, '.mrb')}"
        mrb_dir = File.dirname(mrb_file)
        FileUtils.mkdir_p(mrb_dir)
        mrb_files << mrb_file

        file mrb_file => rb_file do
          puts "  Compiling #{relative_path}..."
          system("#{picorbcfile} -o #{mrb_file} #{rb_file}") or raise "Compilation failed for #{rb_file}"
        end
      end
    end

    mrb_files
  end
end
