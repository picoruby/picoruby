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
        file "#{build_dir}/gem_init.c" => [build.mrbcfile, __FILE__] + [rbfiles].flatten do |t|
          mkdir_p build_dir
          if build.cc.defines.include?("PICORB_VM_MRUBYC") && name.start_with?("picoruby-")
            rbfiles.clear
          end
          generate_gem_init("#{build_dir}/gem_init.c")
        end
      end

      def vm_mruby?
        cc.defines.include?("PICORB_VM_MRUBY")
      end

      def vm_mrubyc?
        cc.defines.include?("PICORB_VM_MRUBYC")
      end

    end
  end
end
