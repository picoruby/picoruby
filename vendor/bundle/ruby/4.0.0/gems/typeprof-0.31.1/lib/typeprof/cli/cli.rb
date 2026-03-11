module TypeProf::CLI
  class CLI
    def initialize(argv)
      opt = OptionParser.new

      opt.banner = "Usage: #{ opt.program_name } [options] files_or_dirs..."

      core_options = {}
      lsp_options = {}
      cli_options = {}

      output = nil
      rbs_collection_path = nil
      initialize_config_file = false

      opt.separator ""
      opt.separator "Options:"
      opt.on("-o OUTFILE", "Output to OUTFILE instead of stdout") {|v| output = v }
      opt.on("-q", "--quiet", "Quiet mode") do
        core_options[:display_indicator] = false
      end
      opt.on("-v", "--verbose", "Verbose mode") do
        core_options[:show_errors] = true
      end
      opt.on("--version", "Display typeprof version") { cli_options[:display_version] = true }
      opt.on("--collection PATH", "File path of collection configuration") { |v| rbs_collection_path = v }
      opt.on("--no-collection", "Ignore collection configuration") { rbs_collection_path = :no }
      opt.on("--lsp", "LSP server mode") do |v|
        core_options[:display_indicator] = false
        cli_options[:lsp] = true
      end

      opt.separator ""
      opt.separator "Analysis output options:"
      opt.on("--[no-]show-typeprof-version", "Display TypeProf version in a header") {|v| core_options[:output_typeprof_version] = v }
      opt.on("--[no-]show-errors", "Display possible errors found during the analysis") {|v| core_options[:output_diagnostics] = v }
      opt.on("--[no-]show-parameter-names", "Display parameter names for methods") {|v| core_options[:output_parameter_names] = v }
      opt.on("--[no-]show-source-locations", "Display definition source locations for methods") {|v| core_options[:output_source_locations] = v }

      opt.separator ""
      opt.separator "Advanced options:"
      opt.on("--[no-]stackprof MODE", /\Acpu|wall|object\z/, "Enable stackprof (for debugging purpose)") {|v| cli_options[:stackprof] = v.to_sym }
      opt.on("--init", 'Generate TypeProf configuration file') {|v| initialize_config_file = true}

      opt.separator ""
      opt.separator "LSP options:"
      opt.on("--port PORT", Integer, "Specify a port number to listen for requests on") {|v| lsp_options[:port] = v }
      opt.on("--stdio", "Use stdio for LSP transport") {|v| lsp_options[:stdio] = v }

      opt.parse!(argv)

      if initialize_config_file
        generate_config_file
        exit 0
      end

      if !cli_options[:lsp] && !lsp_options.empty?
        raise OptionParser::InvalidOption.new("lsp options with non-lsp mode")
      end

      @core_options = {
        rbs_collection: setup_rbs_collection(rbs_collection_path),
        display_indicator: $stderr.tty?,
        output_typeprof_version: true,
        output_errors: false,
        output_parameter_names: false,
        output_source_locations: false,
      }.merge(core_options)

      @lsp_options = {
        port: 0,
        stdio: false,
      }.merge(lsp_options)

      @cli_options = {
        argv:,
        output: output ? open(output, "w") : $stdout.dup,
        display_version: false,
        stackprof: nil,
        lsp: false,
      }.merge(cli_options)

    rescue OptionParser::InvalidOption, OptionParser::MissingArgument
      puts $!
      exit 1
    end

    def setup_rbs_collection(path)
      return nil if path == :no

      unless path
        path = RBS::Collection::Config::PATH.exist? ? RBS::Collection::Config::PATH.to_s : nil
        return nil unless path
      end

      if !File.readable?(path)
        raise OptionParser::InvalidOption.new("file not found: #{ path }")
      end

      lock_path = RBS::Collection::Config.to_lockfile_path(Pathname(path))
      if !File.readable?(lock_path)
        raise OptionParser::InvalidOption.new("file not found: #{ lock_path.to_s }; please run 'rbs collection install")
      end

      RBS::Collection::Config::Lockfile.from_lockfile(lockfile_path: lock_path, data: YAML.load_file(lock_path))
    end

    attr_reader :core_options, :lsp_options, :cli_options

    def run

      if @cli_options[:lsp]
        run_lsp
      else
        run_cli
      end
    end

    def run_lsp
      if @lsp_options[:stdio]
        TypeProf::LSP::Server.start_stdio(@core_options)
      else
        TypeProf::LSP::Server.start_socket(@core_options)
      end
    rescue Exception
      puts $!.detailed_message(highlight: false).gsub(/^/, "---")
      raise
    end

    def run_cli
      core = TypeProf::Core::Service.new(@core_options)

      puts "typeprof #{ TypeProf::VERSION }" if @cli_options[:display_version]

      files = find_files

      set_profiler do
        output = @cli_options[:output]

        core.batch(files, @cli_options[:output])

        output.close
      end

    rescue OptionParser::InvalidOption, OptionParser::MissingArgument
      puts $!
      exit 1
    end

    def find_files
      files = []
      @cli_options[:argv].each do |path|
        if File.directory?(path)
          files.concat(Dir.glob("#{ path }/**/*.{rb,rbs}"))
        elsif File.file?(path)
          files << path
        else
          raise OptionParser::InvalidOption.new("no such file or directory -- #{ path }")
        end
      end

      if files.empty?
        exit if @cli_options[:display_version]
        raise OptionParser::InvalidOption.new("no input files")
      end

      files
    end

    def set_profiler
      if @cli_options[:stackprof]
        require "stackprof"
        out = "typeprof-stackprof-#{ @cli_options[:stackprof] }.dump"
        StackProf.start(mode: @cli_options[:stackprof], out: out, raw: true)
      end

      yield

    ensure
      if @cli_options[:stackprof] && defined?(StackProf)
        StackProf.stop
        StackProf.results
      end
    end

    def generate_config_file
      exist_dirs = ["app", "lib"].select { |dir| File.exist?(File.join(Dir.pwd, dir)) }
      File.write('typeprof.conf.jsonc', <<~JSONC, mode: "wx")
        {
          "typeprof_version": "experimental",
          "rbs_dir": "sig/",
          "analysis_unit_dirs": #{exist_dirs.inspect}
          // "diagnostic_severity": "warning"
        }
      JSONC
    end
  end
end
