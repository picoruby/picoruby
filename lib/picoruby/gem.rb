module MRuby
  module Gem
    class Specification
      attr_accessor :require_name

      alias_method :original_setup_compilers, :setup_compilers
      def setup_compilers
        original_setup_compilers
        return unless cc.build.posix?
        # setup for POSIX
        ["posix", "common"].each do |subdir|
          Dir.glob("#{dir}/ports/#{subdir}/**/*.c").each do |src|
            obj = objfile(src.pathmap("#{build_dir}/ports/#{subdir}/%n"))
            build.libmruby_objs << obj
            file obj => src do |f|
              cc.run f.name, f.prerequisites.first
            end
          end
        end
      end

      def define_gem_init_builder
        fname = "#{build_dir}/gem_init.c"
        generated_file fname, [build.mrbcfile, __FILE__] + [rbfiles].flatten, inputs: [cdump?, *objs] do |f|
          _pp "GEN", fname.relative_path
          if build.cc.defines.include?("PICORB_VM_MRUBYC") && name.start_with?("picoruby-")
            rbfiles.clear
          end
          generate_gem_init(f)
        end
      end

    end
  end
end
