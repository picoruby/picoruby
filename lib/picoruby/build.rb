require "etc"

module MRuby
  class Command::Compiler
    attr_accessor :host_command
  end

  class Build
    # Override
    def build_mrbc_exec
      gem core: 'mruby-compiler' unless @gems['mruby-compiler']
      gem core: "mruby-bin-mrbc" unless @gems['mruby-bin-mrbc']
      # MRuby.resolve_mrbc_hosts generates a bare MRuby::Build for a cross
      # target's mrbc, which never goes through #common; mruby-compiler's
      # headers include <mrbconf.h> from the mruby core in the submodule.
      mruby_include = "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/include"
      cc.include_paths << mruby_include unless cc.include_paths.include?(mruby_include)
      self.mrbcfile = "#{build_dir}/bin/mrbc"
      set_build_info
    end

    alias_method :original_create_mrbc_build, :create_mrbc_build

    def create_mrbc_build
      # mrbc is a standalone compiler; strip VM-specific defines so
      # the sub-build does not pull in mruby.h (and presym/id.h).
      vm_defines = %w[PICORB_VM_MRUBY MRB_USE_TASK_SCHEDULER]
      saved = vm_defines.select { |d| cc.defines.include?(d) }
      saved.each { |d| cc.defines.delete(d) }
      build = original_create_mrbc_build
      saved.each { |d| cc.defines << d }
      build
    end

    def picoruby?
      cc.defines.include?("PICORB_VM_MRUBY")
    end
    alias_method :vm_mruby?, :picoruby?

    def femtoruby?
      cc.defines.include?("PICORB_VM_MRUBYC")
    end
    alias_method :vm_mrubyc?, :femtoruby?

    def common
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/mruby-compiler/include"
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/mruby-compiler/lib/prism/include"
      # Prism's generated headers (ast.h, diagnostic.h) are written under
      # build_root, not into the submodule, so every gem needs this path too.
      cc.include_paths << "#{build_root}/prism/include"
      # Workaround: To avoid error in compiling gem_init.c
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/include"
      # Pass PICORUBY_VERSION to mruby-compiler
      version = File.read("#{MRUBY_ROOT}/include/version.h").match(/#define PICORUBY_VERSION "(.+?)"/)[1]
      cc.defines << "PICORUBY_VERSION=\\\"#{version}\\\""
      set_build_info
    end

    def set_build_info
      return if cc.defines.any? { _1.start_with?("MRC_COMMIT_TIMESTAMP=") }
      timestamp = Time.at(`git log -1 --format=%ct`.to_i).utc.strftime('%Y-%m-%dT%H:%M:%SZ')
      branch = `git branch --show-current`.strip
      commit_hash = `git log -1 --format=%h`.strip
      cc.defines << "MRC_COMMIT_TIMESTAMP=\\\"#{timestamp}\\\""
      cc.defines << "MRC_COMMIT_BRANCH=\\\"#{branch}\\\""
      cc.defines << "MRC_COMMIT_HASH=\\\"#{commit_hash}\\\""
    end

    def rite_version
      mrc_dump = File.read("#{MRUBY_ROOT}/mrbgems/mruby-compiler/include/mrc_dump.h")
      ident = mrc_dump[/#define RITE_BINARY_IDENT\s+"(.+?)"/, 1]
      major = mrc_dump[/#define RITE_BINARY_MAJOR_VER\s+"(.+?)"/, 1]
      minor = mrc_dump[/#define RITE_BINARY_MINOR_VER\s+"(.+?)"/, 1]
      "#{ident}#{major}#{minor}"
    end

    def add_define_once(define)
      key, = define.split("=")
      cc.defines << define if cc.defines.none? { _1 == key || _1.start_with?("#{key}=") }
    end

    # Spinel-typed AOT of mrblib methods into mruby cfuncs (mrbgems/picoruby-aot).
    # This is the per-target switch; a gem still has to opt in with
    # `conf.gem(core: 'x') { |g| g.aot = true }`. Without this call nothing
    # about the build changes.
    #
    #   conf.aot                              # the Spinel submodule, built in place
    #   conf.aot path: "/opt/spinel/spinel"   # an explicit binary
    #
    # $SPINEL=/path/to/spinel overrides the submodule for a developer who
    # builds Spinel from another checkout; `path:` overrides both.
    #
    # Spinel (github.com/matz/spinel) is the submodule at SPINEL_DIR, pinned
    # to the commit the twin tests passed with, and taken as is: a change
    # goes upstream first, then the submodule moves. A build that does not
    # call this never touches it. The first build runs `make deps` there
    # (the vendored parsers, fetched once from rubygems.org), and every
    # build runs `make`, which is a no-op when nothing moved, so a bumped
    # submodule rebuilds itself and the gems regenerate.
    SPINEL_DIR = "#{MRUBY_ROOT}/mrbgems/picoruby-aot/lib/spinel"

    def aot(path: nil)
      path ||= ENV["SPINEL"]
      prereqs = []
      if path.nil?
        path = "#{SPINEL_DIR}/bin/spinel"
        prereqs << path
        define_spinel_submodule_task(path)
      end
      @aot = { path: path, prereqs: prereqs }
      gem core: 'picoruby-aot' unless @gems['picoruby-aot']
    end

    def aot?
      !@aot.nil?
    end

    def aot_spinel
      @aot[:path]
    end

    # File tasks the AOT generation of every gem depends on: the submodule
    # binary when the build manages one, nothing for $SPINEL or `path:`.
    # A gem regenerates only when that binary's timestamp moved.
    def aot_spinel_prereqs
      @aot ? @aot[:prereqs] : []
    end

    # The binary is a file task whose only prerequisite is the submodule's
    # HEAD file, which git rewrites when the checkout moves. A plain task
    # would not do: Rake takes every transitive prerequisite into the
    # out-of-date test, and a plain task is always "now", so the gems would
    # regenerate on every build. A source edited in place inside the
    # submodule is not seen; build it with `make` there, or use $SPINEL.
    def define_spinel_submodule_task(bin)
      return if Rake::Task.task_defined?(bin)
      dir = SPINEL_DIR
      hint = "spinel: the submodule #{dir.relative_path} is empty; run\n" \
             "  git submodule update --init --depth 1 #{dir.relative_path}"
      unless File.exist?("#{dir}/Makefile")
        file bin do
          fail hint
        end
        return
      end
      git_dir = `git -C "#{dir}" rev-parse --absolute-git-dir 2>/dev/null`.strip
      head = File.exist?("#{git_dir}/HEAD") ? ["#{git_dir}/HEAD"] : []
      jobs = [Etc.nprocessors, 8].min
      file bin => head do
        mkdir_p "#{dir}/build"
        unless File.exist?("#{dir}/vendor/prism/include/prism.h")
          _pp "MAKE", "#{dir.relative_path} deps (the vendored parsers, once)"
          sh "make -C \"#{dir}\" deps > \"#{dir}/build/make-deps.log\" 2>&1 || (tail -20 \"#{dir}/build/make-deps.log\"; exit 1)"
        end
        _pp "MAKE", dir.relative_path
        sh "make -C \"#{dir}\" -j#{jobs} > \"#{dir}/build/make.log\" 2>&1 || (tail -20 \"#{dir}/build/make.log\"; exit 1)"
        fail "spinel: #{bin} was not built" unless File.executable?(bin)
        touch bin
      end
    end

    def picoruby(alloc_estalloc: true, alloc_align: 8)
      # Place picoruby-mruby at the top so that Kernel#require is defined first
      if gems.first
        gems.first.add_dependency 'picoruby-mruby'
      else
        gem core: 'picoruby-mruby'
      end
      common
      if alloc_estalloc
        add_define_once "PICORB_ALLOC_ESTALLOC"
        add_define_once "PICORB_ALLOC_ALIGN=#{alloc_align}"
      end
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/include"
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-task/include"
      cc.defines << "PICORB_VM_MRUBY"
      cc.defines << "MRB_USE_TASK_SCHEDULER"

      timestamp = Time.at(`git log -1 --format=%ct`.to_i).utc.strftime('%Y-%m-%dT%H:%M:%SZ')
      commit_hash = `git log -1 --format=%h`.strip
      build_date = Time.now.utc.strftime("%Y-%m-%d")

      File.write(
        "#{MRUBY_ROOT}/src/version.c",
        File.read("#{MRUBY_ROOT}/src/version.c.in")
            .gsub('@PICORUBY_COMMIT_TIMESTAMP@', timestamp)
            .gsub('@PICORUBY_COMMIT_HASH@', commit_hash)
            .gsub('@PICORUBY_BUILD_DATE@', build_date)
            .gsub('@RITE_VERSION@', rite_version)
      )

      debug_flag
    end

    def femtoruby(alloc_libc: true, alloc_estalloc: true, alloc_align: 8)
      common

      # Override by environment variable
      alloc_libc = false if ENV["PICORB_NO_LIBC_ALLOC"]

      if alloc_libc && alloc_estalloc
        add_define_once "PICORB_ALLOC_ESTALLOC"
        add_define_once "PICORB_ALLOC_ALIGN=#{alloc_align}"
        if cc.defines.include?("PICORB_PLATFORM_RP2")
          add_define_once "PICORB_WRAP_LIBC_ALLOC"
        end
      end

      cc.defines << "PICORB_VM_MRUBYC"
      cc.defines << "MRBC_SCHEDULER_EXIT=1"
      cc.defines << "MRBC_NO_STDIO" # skip implementing methods like c_object_puts
      add_define_once(alloc_libc ? "MRBC_ALLOC_LIBC" : "MRBC_USE_ALLOC_PROF")
      cc.defines << "DISABLE_MRUBY"
      cc.defines << "MRBC_INT64" if cc.defines.include?("PICORB_INT64")
      %w(MRBC_USE_MATH=1 MAX_SYMBOLS_COUNT=1000 MAX_VM_COUNT=255 MAX_REGS_SIZE=255).each do |define|
        key, _value = define.split("=")
        cc.defines << define if cc.defines.none? { _1.start_with? key }
      end

      timestamp = Time.at(`git log -1 --format=%ct`.to_i).utc.strftime('%Y-%m-%dT%H:%M:%SZ')
      commit_hash = `git log -1 --format=%h`.strip
      build_date = Time.now.utc.strftime("%Y-%m-%d")

      File.write(
        "#{MRUBY_ROOT}/src/version.c",
        File.read("#{MRUBY_ROOT}/src/version.c.in")
            .gsub('@PICORUBY_COMMIT_TIMESTAMP@', timestamp)
            .gsub('@PICORUBY_COMMIT_HASH@', commit_hash)
            .gsub('@PICORUBY_BUILD_DATE@', build_date)
            .gsub('@RITE_VERSION@', rite_version)
      )

      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-machine/include"
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mrubyc/include"
      cc.include_paths << "#{MRUBY_ROOT}/include/picoruby"
      cc.include_paths << "#{build_dir}/mrbgems" # for `#include <picogem_init.c>`
      cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mrubyc/lib/mrubyc/src"

      debug_flag
    end

    def platform?(name)
      cc.defines.include?("PICORB_PLATFORM_#{name.to_s.upcase}")
    end

    def posix?
      platform?(:posix)
    end

    def wasm?
      config = ENV['CONFIG'] || ENV['MRUBY_CONFIG']
      return true if config&.include?('picoruby-wasm')
      # Also check defines for WASM platform
      platform?(:wasm)
    end

    def generate_package_json_from_template(template_path, output_path)
      return unless File.exist?(template_path)
      version = File.read("#{MRUBY_ROOT}/include/version.h")
                    .match(/#define PICORUBY_VERSION "(.+?)"/)[1]
      template_content = File.read(template_path)
      generated_content = template_content.gsub('{{VERSION}}', version)
      File.write(output_path, generated_content)
    end

    private def debug_flag
      cc.flags.flatten!
      cc.flags.reject! { |f| %w(-g -g1 -g2 -g3 -O0 -O1 -O2 -O3).include? f }
      if ENV["PICORB_DEBUG"]
        cc.defines << "PICORB_DEBUG=1"
        if cc.command == "emcc"
          # At -O0 every temporary of mrb_vm_exec() is a wasm local, and with
          # word boxing (mrb_value is one word) that is about 46k of them;
          # binaryen refuses a function past 50k locals before wasm-opt gets
          # to shrink it. -O1 keeps the count small. No C-level debugger is
          # used on wasm; PICORB_DEBUG is about assertions and the mruby
          # debug hook, which -O1 leaves intact.
          cc.flags << "-O1"
          cc.flags << "-g"
        else
          cc.flags << "-O0"
          cc.flags << "-g3"
        end
        cc.flags << "-fno-inline"
      else
        cc.defines << "NDEBUG=1"
        cc.flags << "-Os"
        cc.flags << "-finline-functions"
        cc.flags << "-ffunction-sections"
        cc.flags << "-fdata-sections"
        cc.flags << "-fomit-frame-pointer"
        # cc.flags << "-flto" # Build fails with -flto
        unless cc.command == "clang" || cc.command == "emcc"
          linker.flags << "-s" unless RUBY_PLATFORM.include?("darwin")
          linker.flags << (RUBY_PLATFORM.include?("darwin") ? "-Wl,-dead_strip" : "-Wl,--gc-sections")
        end
      end
    end

  end

end
