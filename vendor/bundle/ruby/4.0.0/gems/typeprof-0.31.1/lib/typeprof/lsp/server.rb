require "cgi/escape"
require "cgi/util" if RUBY_VERSION < "3.5"

module TypeProf::LSP
  module ErrorCodes
    ParseError = -32700
    InvalidRequest = -32600
    MethodNotFound = -32601
    InvalidParams = -32602
    InternalError = -32603
  end

  class Server
    def self.start_stdio(core_options)
      $stdin.binmode
      $stdout.binmode
      reader = Reader.new($stdin)
      writer = Writer.new($stdout)
      # pipe all builtin print output to stderr to avoid conflicting with lsp
      $stdout = $stderr
      new(core_options, reader, writer).run
    end

    def self.start_socket(core_options)
      Socket.tcp_server_sockets("localhost", nil) do |servs|
        serv = servs[0].local_address
        $stdout << JSON.generate({
          host: serv.ip_address,
          port: serv.ip_port,
          pid: $$,
        })
        $stdout.flush

        $stdout = $stderr

        Socket.accept_loop(servs) do |sock|
          sock.set_encoding("UTF-8")
          begin
            reader = Reader.new(sock)
            writer = Writer.new(sock)
            new(core_options, reader, writer).run
          ensure
            sock.close
          end
          exit
        end
      end
    end

    def initialize(core_options, reader, writer, url_schema: nil)
      @core_options = core_options
      @cores = {}
      @reader = reader
      @writer = writer
      @request_id = 0
      @running_requests_from_client = {}
      @running_requests_from_server = {}
      @open_texts = {}
      @exit = false
      @signature_enabled = true
      @url_schema = url_schema || (File::ALT_SEPARATOR != "\\" ? "file://" : "file:///")
      @diagnostic_severity = :error
    end

    attr_reader :open_texts
    attr_accessor :signature_enabled

    #: (String) -> String
    def path_to_uri(path)
      @url_schema + File.expand_path(path).split("/").map {|s| CGI.escapeURIComponent(s) }.join("/")
    end

    def uri_to_path(uri)
      uri.delete_prefix(@url_schema).split("/").map {|s| CGI.unescapeURIComponent(s) }.join("/")
    end

    #: (Array[String]) -> void
    def add_workspaces(folders)
      folders.each do |path|
        conf_path = [".json", ".jsonc"].map do |ext|
          File.join(path, "typeprof.conf" + ext)
        end.find do |path|
          File.readable?(path)
        end
        unless conf_path
          puts "typeprof.conf.json is not found in #{ path }"
          next
        end
        conf = TypeProf::LSP.load_json_with_comments(conf_path, symbolize_names: true)
        if conf
          if conf[:rbs_dir]
            rbs_dir = File.expand_path(conf[:rbs_dir])
          else
            rbs_dir = File.expand_path(File.expand_path("sig", path))
          end
          @rbs_dir = rbs_dir
          if conf[:typeprof_version] == "experimental"
            if conf[:diagnostic_severity]
              severity = conf[:diagnostic_severity].to_sym
              case severity
              when :error, :warning, :info, :hint
                @diagnostic_severity = severity
              else
                puts "unknown severity: #{ severity }"
              end
            end
            conf[:analysis_unit_dirs].each do |dir|
              dir = File.expand_path(dir, path)
              core = @cores[dir] = TypeProf::Core::Service.new(@core_options)
              core.add_workspace(dir, @rbs_dir)
            end
          else
            puts "Unknown typeprof_version: #{ conf[:typeprof_version] }"
          end
        end
      end
    end

    #: (String) -> bool
    def target_path?(path)
      return true if @rbs_dir && path.start_with?(@rbs_dir)
      @cores.each do |folder, _|
        return true if path.start_with?(folder)
      end
      return false
    end

    def each_core(path)
      @cores.each do |folder, core|
        if path.start_with?(folder) || @rbs_dir && path.start_with?(@rbs_dir)
          yield core
        end
      end
    end

    def aggregate_each_core(path)
      ret = []
      each_core(path) do |core|
        r = yield(core)
        ret.concat(r) if r
      end
      ret
    end

    def update_file(path, text)
      each_core(path) do |core|
        core.update_file(path, text)
      end
    end

    def definitions(path, pos)
      aggregate_each_core(path) do |core|
        core.definitions(path, pos)
      end
    end

    def type_definitions(path, pos)
      aggregate_each_core(path) do |core|
        core.type_definitions(path, pos)
      end
    end

    def references(path, pos)
      aggregate_each_core(path) do |core|
        core.references(path, pos)
      end
    end

    def hover(path, pos)
      ret = []
      each_core(path) do |core|
        ret << core.hover(path, pos)
      end
      ret.compact.first # TODO
    end

    def code_lens(path, &blk)
      each_core(path) do |core|
        core.code_lens(path, &blk)
      end
    end

    def completion(path, trigger, pos, &blk)
      each_core(path) do |core|
        core.completion(path, trigger, pos, &blk)
      end
    end

    def rename(path, pos)
      aggregate_each_core(path) do |core|
        core.rename(path, pos)
      end
    end

    def run
      @reader.read do |json|
        if json[:method]
          # request or notification
          msg_class = Message.find(json[:method])
          if msg_class
            msg = msg_class.new(self, json)
            @running_requests_from_client[json[:id]] = msg if json[:id]
            msg.run
          else

          end
        else
          # response
          callback = @running_requests_from_server.delete(json[:id])
          callback&.call(json[:params], json[:error])
        end
        break if @exit
      end
    end

    def send_response(**msg)
      @running_requests_from_client.delete(msg[:id])
      @writer.write(**msg)
    end

    def send_notification(method, **params)
      @writer.write(method: method, params: params)
    end

    def send_request(method, **params, &blk)
      id = @request_id += 1
      @running_requests_from_server[id] = blk
      @writer.write(id: id, method: method, params: params)
    end

    def cancel_request(id)
      req = @running_requests_from_client[id]
      req.cancel if req.respond_to?(:cancel)
    end

    def exit
      @exit = true
    end

    def publish_updated_diagnostics
      @cores.each do |_, core|
        diags = []
        core.process_diagnostic_paths do |path|
          uri = path_to_uri(path)
          next false unless @open_texts[uri]
          core.diagnostics(path) do |diag|
            diags << diag.to_lsp(severity: @diagnostic_severity)
          end
          send_notification(
            "textDocument/publishDiagnostics",
            uri: uri,
            diagnostics: diags
          )
          true
        end
      end
    end
  end

  class Reader
    class ProtocolError < StandardError
    end

    def initialize(io)
      @io = io
    end

    def read
      while line = @io.gets
        line2 = @io.gets
        if line =~ /\AContent-length: (\d+)\r\n\z/i && line2 == "\r\n"
          len = $1.to_i
          json = JSON.parse(@io.read(len), symbolize_names: true)
          yield json
        else
          raise ProtocolError, "LSP broken header"
        end
      end
    end
  end

  class Writer
    def initialize(io)
      @io = io
      @mutex = Mutex.new
    end

    def write(**json)
      json = JSON.generate(json.merge(jsonrpc: "2.0"))
      @mutex.synchronize do
        @io << "Content-Length: #{ json.bytesize }\r\n\r\n" << json
        @io.flush
      end
    end
  end
end
