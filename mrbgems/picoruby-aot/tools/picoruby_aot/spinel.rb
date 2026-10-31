# Driving the spinel binary as a type oracle.
#
# Spinel compiles a whole program. For a gem we hand it one entry file that
# loads the generated stubs, the gem's dependencies and the gem itself,
# plus a feature root so `require "name"` resolves to the right mrblib, and
# a merged directory of every RBS file as the inference seed. We ask for
# `--emit-types` only: no C, no cc.

require "fileutils"
require "json"
require "pathname"
require_relative "gems"
require_relative "stubs"
require_relative "engine_fold"

module PicorubyAot
  # Two ways to show a gem's dependencies to Spinel:
  #
  # - :stubs (default): every other gem is represented only by Ruby stubs
  #   generated from its RBS. The gem boundary is the RBS contract, and a
  #   Spinel refusal inside some other gem's source cannot sink this gem's
  #   analysis. `require "name"` resolves to an empty feature file.
  # - :source: the dependency closure's mrblib sources are loaded too, so
  #   inference can see through the boundary. Slower and more fragile.
  class Workspace
    attr_reader :root

    def initialize(graph, root)
      @graph = graph
      @root = File.expand_path(root)
    end

    def feature_root(mode)
      "#{@root}/features_#{mode}"
    end

    # Rebuild the shared parts of the workspace from the gem graph.
    def prepare
      FileUtils.rm_rf(@root)
      FileUtils.mkdir_p(feature_root(:stubs))
      FileUtils.mkdir_p(feature_root(:source))
      @graph.each do |g|
        next if g.rbfiles.empty?
        [:stubs, :source].each do |mode|
          path = File.join(feature_root(mode), g.require_name + ".rb")
          FileUtils.mkdir_p(File.dirname(path))
          body = mode == :source ? g.rbfiles.map { |f| "require_relative #{rel(copy_of(g, f), path).inspect}\n" }.join : "# #{g.name}: provided by RBS stubs\n"
          File.write(path, body)
        end
      end
      self
    end

    # The engine the generated C runs on, which the workspace folds the
    # RUBY_ENGINE branches of every source for (EngineFold).
    ENGINE = "mruby"

    # The copy of mrblib file `f` of gem `g` that Spinel reads: the source
    # with its RUBY_ENGINE branches folded, under src/<gem>/, refreshed when
    # the source is newer. Positions in the copy are positions in the source.
    def copy_of(g, f)
      copy = File.join(@root, "src", g.name, f.sub(g.dir + "/", ""))
      unless File.exist?(copy) && File.mtime(copy) >= File.mtime(f)
        FileUtils.mkdir_p(File.dirname(copy))
        File.write(copy, EngineFold.fold(File.read(f), engine: ENGINE))
      end
      copy
    end

    # The mrblib file a path in Spinel's output names: the source of a copy
    # under src/, any other path as it is.
    def source_of(path)
      full = File.expand_path(path)
      prefix = File.join(@root, "src") + "/"
      return path unless full.start_with?(prefix)
      gem_name, rest = full.sub(prefix, "").split("/", 2)
      g = @graph[gem_name]
      g ? File.join(g.dir, rest) : path
    end

    # Gems whose RBS every analysis sees, on top of the dependency closure:
    # the VM's own classes (Task, Object additions) that no gem declares a
    # dependency on.
    BASE_GEMS = %w[picoruby-mruby].freeze

    Entry = Struct.new(:path, :sig_dir, :stub_file, :gems)

    # One entry per gem: a sig dir holding the RBS of the gem, its
    # dependency closure and BASE_GEMS; stubs for everything in that RBS the
    # gem's own Ruby does not define; then (in :source mode) the closure's
    # sources; then the gem itself. Keeping the RBS to the closure matters:
    # a contradiction inside an unrelated gem's signatures would otherwise
    # abort this gem's analysis.
    def entry_file(gem_name, mode: :stubs)
      g = @graph[gem_name]
      FileUtils.mkdir_p("#{@root}/entry")
      FileUtils.mkdir_p("#{@root}/stubs")
      gems = (@graph.closure(gem_name) + BASE_GEMS.map { |n| @graph[n] }).uniq
      sig_dir = "#{@root}/sig/#{gem_name}"
      FileUtils.rm_rf(sig_dir)
      FileUtils.mkdir_p(sig_dir)
      gems.each do |d|
        d.sigfiles.each { |s| File.symlink(s, File.join(sig_dir, "#{d.name}__#{File.basename(s)}")) }
      end
      own = mode == :source ? gems.flat_map(&:rbfiles) : g.rbfiles
      defs = RubyDefs.new.add_files(own)
      stub_file = "#{@root}/stubs/#{gem_name}.rb"
      File.write(stub_file, StubGenerator.new(gems.flat_map(&:sigfiles), defs).generate)
      path = "#{@root}/entry/#{gem_name}.rb"
      lines = ["require_relative #{rel(stub_file, path).inspect}"]
      if mode == :source
        gems.reject { |d| d.name == gem_name }.each do |d|
          d.rbfiles.each { |f| lines << "require_relative #{rel(copy_of(d, f), path).inspect}" }
        end
      end
      g.rbfiles.each { |f| lines << "require_relative #{rel(copy_of(g, f), path).inspect}" }
      File.write(path, lines.join("\n") + "\n")
      Entry.new(path, sig_dir, stub_file, gems)
    end

    private

    # A require_relative target as a path relative to the requiring file.
    # Spinel 2026.09.12 joins even an absolute target onto the requiring
    # file's directory, so an absolute path is not portable across releases.
    def rel(target, from_file)
      Pathname.new(target).relative_path_from(Pathname.new(File.dirname(from_file))).to_s
    end
  end

  class SpinelRunner
    attr_reader :binary

    def initialize(binary = ENV["SPINEL"] || "spinel")
      @binary = binary
    end

    def version
      `#{@binary} --version`.strip
    end

    # Returns [json_hash, log_text, exit_status].
    def emit_types(entry, workspace, out_json, mode: :stubs)
      cmd = [@binary, entry.path, "-I", workspace.feature_root(mode),
             "--rbs", entry.sig_dir, "--emit-types", "-o", out_json, "--force"]
      log = IO.popen(cmd, err: [:child, :out], &:read)
      status = $?.exitstatus
      json = File.exist?(out_json) ? JSON.parse(File.read(out_json)) : nil
      # Spinel read the folded copies; every record names the source instead,
      # in the file too, so what is kept under out/ reads as the generator did.
      if json
        %w[types diagnostics codegen].each do |k|
          (json[k] || []).each { |r| r["file"] = workspace.source_of(r["file"]) if r["file"] }
        end
        File.write(out_json, JSON.generate(json))
      end
      [json, log, status]
    end
  end

  # What the JSON says about one gem.
  class Analysis
    attr_reader :gem, :json, :log, :status

    def initialize(gem, json, log, status)
      @gem = gem
      @json = json || { "types" => [], "diagnostics" => [], "codegen" => [] }
      @log = log
      @status = status
    end

    def own?(rec)
      f = rec["file"]
      f && @gem.rbfiles.include?(File.expand_path(f))
    end

    def defs
      @json["types"].select { |r| r["kind"] == "DefNode" && own?(r) }
    end

    def widened_defs
      defs.select { |r| r["widened"] }
    end

    def errors
      @json["diagnostics"].select { |r| r["severity"] == "error" }
    end

    def own_warnings
      @json["diagnostics"].select { |r| r["severity"] == "warning" && own?(r) }
    end

    def summary
      d = defs
      w = widened_defs
      {
        gem: @gem.name,
        status: @status,
        rb_lines: @gem.rbfiles.sum { |f| File.foreach(f).count },
        defs: d.size,
        typed: d.size - w.size,
        widened: w.size,
        errors: errors.size,
        own_warnings: own_warnings.size,
      }
    end
  end
end
