module MRuby
  module Gem
    class Specification
      attr_accessor :require_name

      # picoruby-aot: `g.aot = true` in the build config lets this gem's typed
      # mrblib methods be compiled to cfuncs. `aot_exclude` names methods
      # ("Klass#meth", "Klass.meth") that stay in the irep regardless.
      attr_accessor :aot, :aot_exclude

      def aot?
        !!aot && build.respond_to?(:aot?) && build.aot? && build.picoruby?
      end

      def aot_dir
        "#{build_dir}/aot"
      end

      def aot_c
        "#{aot_dir}/aot_#{funcname}.c"
      end

      # The Ruby left for mrbc once the compiled defs are blanked out; one
      # file per mrblib file, same relative name.
      def aot_rbfiles
        rbfiles.map { |f| "#{aot_dir}/rb/#{f.sub("#{dir}/mrblib/", '')}" }
      end

      def setup_aot
        tools_dir = "#{MRUBY_ROOT}/mrbgems/picoruby-aot/tools"
        tool = "#{tools_dir}/aot-gen"
        tool_srcs = Dir.glob("#{tools_dir}/**/*.rb") + [tool]
        sigfiles = Dir.glob("#{dir}/sig/**/*.rbs").sort
        exclude = (aot_exclude || []).empty? ? "" : " --exclude #{(aot_exclude || []).join(',')}"
        workspace = "#{build.build_dir}/aot-workspace"
        manifest = "#{aot_dir}/manifest.json"
        file aot_c => rbfiles + sigfiles + tool_srcs do |t|
          _pp "AOT", aot_c.relative_path
          mkdir_p aot_dir
          sh %Q[SPINEL="#{build.aot_spinel}" ruby "#{tool}" --gem #{name} --workspace "#{workspace}" --out-c "#{aot_c}" --out-rb "#{aot_dir}/rb" --manifest "#{manifest}"#{exclude}]
        end
        aot_rbfiles.each { |f| file f => aot_c }
        cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-aot/include"
        obj = objfile("#{aot_dir}/aot_#{funcname}")
        file obj => aot_c do |f|
          cc.run f.name, aot_c
        end
        # @objs is the array already registered in build.libmruby_objs.
        @objs << obj
      end

      alias_method :original_setup_compilers, :setup_compilers
      def setup_compilers
        setup_aot if aot?
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
        return define_aot_gem_init_builder if aot?
        fname = "#{build_dir}/gem_init.c"
        generated_file fname, [build.mrbcfile, __FILE__] + [rbfiles].flatten, inputs: [cdump?, *objs] do |f|
          _pp "GEN", fname.relative_path
          if build.cc.defines.include?("PICORB_VM_MRUBYC") && name.start_with?("picoruby-")
            rbfiles.clear
          end
          generate_gem_init(f)
        end
      end

      # The AOT variant of gem_init.c: mrbc compiles the stripped Ruby, and
      # the compiled cfuncs are registered right after the irep is loaded,
      # so the classes the Ruby opens exist when the methods are added.
      def define_aot_gem_init_builder
        fname = "#{build_dir}/gem_init.c"
        generated_file fname, [build.mrbcfile, __FILE__] + aot_rbfiles, inputs: [cdump?, *objs] do |f|
          _pp "GEN", fname.relative_path
          original = rbfiles
          @rbfiles = aot_rbfiles
          # generate_gem_init takes any object beyond gem_init.o as proof of
          # a hand-written mrb_<gem>_gem_init; the AOT object is not one.
          aot_obj = objfile("#{aot_dir}/aot_#{funcname}")
          @objs.delete(aot_obj)
          scratch = "#{aot_dir}/gem_init.scratch.c"
          File.open(scratch, "w") { |s| generate_gem_init(s) }
          @objs << aot_obj
          @rbfiles = original
          src = File.read(scratch)
          rm_f scratch
          register = "mrb_aot_#{funcname}_register"
          src.sub!(/^void mrb_#{funcname}_gem_init\(mrb_state \*mrb\);$/) { |m| "void #{register}(mrb_state *mrb);\n#{m}" }
          src.sub!(/^(  mrb_load_(?:irep|proc)\(mrb, gem_mrblib_[^\n]*\);)$/) { |m| "#{m}\n  #{register}(mrb);" }
          f.write src
        end
      end

    end
  end
end
