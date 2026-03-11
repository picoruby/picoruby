module TypeProf::LSP
  class Message
    #: (Server, untyped) -> void
    def initialize(server, json)
      @server = server
      @id = json[:id]
      @method = json[:method]
      @params = json[:params]
    end

    def run
      p [:ignored, @method]
    end

    def log(msg)
    end

    def respond(result)
      raise "do not respond to notification" if @id == nil
      @server.send_response(id: @id, result: result)
    end

    def respond_error(error)
      raise "do not respond to notification" if @id == nil
      @server.send_response(id: @id, error: error)
    end

    def notify(method, **params)
      @server.send_notification(method, **params)
    end

    Classes = []
    def self.inherited(klass)
      Classes << klass
    end

    Table = Hash.new(Message)
    def self.build_table
      Classes.each do |klass|
        Table[klass::METHOD] = klass
      end
    end

    def self.find(method)
      Table[method]
    end
  end

  class Message::CancelRequest < Message
    METHOD = "$/cancelRequest" # notification
    def run
      @server.cancel_request(@params[:id])
    end
  end

  class Message::Initialize < Message
    METHOD = "initialize" # request (required)
    def run
      folders = @params[:workspaceFolders].map do |folder|
        folder => { uri:, }
        @server.uri_to_path(uri)
      end

      @server.add_workspaces(folders)

      respond(
        capabilities: {
          textDocumentSync: {
            openClose: true,
            change: 2, # Incremental
          },
          hoverProvider: true,
          definitionProvider: true,
          typeDefinitionProvider: true,
          completionProvider: {
            triggerCharacters: [".", ":"],
          },
          #signatureHelpProvider: {
          #  triggerCharacters: ["(", ","],
          #},
          codeLensProvider: {
            resolveProvider: false,
          },
          renameProvider: {
            prepareProvider: false,
          },
          executeCommandProvider: {
            commands: [
              "typeprof.createPrototypeRBS",
              "typeprof.enableSignature",
              "typeprof.disableSignature",
            ],
          },
          referencesProvider: true,
        },
        serverInfo: {
          name: "typeprof",
          version: TypeProf::VERSION,
        },
      )

      log "TypeProf for IDE is started successfully"
    end
  end

  class Message::Initialized < Message
    METHOD = "initialized" # notification
    def run
    end
  end

  class Message::Shutdown < Message
    METHOD = "shutdown" # request (required)
    def run
      respond(nil)
    end
  end

  class Message::Exit < Message
    METHOD = "exit" # notification
    def run
      @server.exit
    end
  end

  module Message::TextDocument
  end

  class Message::TextDocument::DidOpen < Message
    METHOD = "textDocument/didOpen" # notification
    def run
      @params => { textDocument: { uri:, version:, text: } }

      path = @server.uri_to_path(uri)
      return unless @server.target_path?(path)

      text = Text.new(path, text, version)
      @server.open_texts[uri] = text
      @server.update_file(text.path, text.string)
      notify("typeprof.enableToggleButton")
      @server.send_request("workspace/codeLens/refresh")
      @server.publish_updated_diagnostics
    end
  end

  class Message::TextDocument::DidChange < Message
    METHOD = "textDocument/didChange" # notification
    def run
      @params => { textDocument: { uri:, version: }, contentChanges: changes }

      text = @server.open_texts[uri]
      return unless text

      text.apply_changes(changes, version)
      @server.update_file(text.path, text.string)
      @server.send_request("workspace/codeLens/refresh")
      @server.publish_updated_diagnostics
    end
  end

  # textDocument/willSave notification
  # textDocument/willSaveWaitUntil request
  # textDocument/didSave notification

  class Message::TextDocument::DidClose < Message
    METHOD = "textDocument/didClose" # notification
    def run
      @params => { textDocument: { uri: } }

      text = @server.open_texts.delete(uri)
      return unless text

      @server.update_file(text.path, nil)
    end
  end

  # textDocument/declaration request

  class Message::TextDocument::Definition < Message
    METHOD = "textDocument/definition" # request
    def run
      @params => {
        textDocument: { uri: },
        position: pos,
      }

      text = @server.open_texts[uri]
      unless text
        respond(nil)
        return
      end
      defs = @server.definitions(text.path, TypeProf::CodePosition.from_lsp(pos))
      if defs.empty?
        respond(nil)
      else
        respond(defs.map do |path, code_range|
          {
            uri: @server.path_to_uri(path),
            range: code_range.to_lsp,
          }
        end)
      end
    end
  end

  class Message::TextDocument::TypeDefinition < Message
    METHOD = "textDocument/typeDefinition" # request
    def run
      @params => {
        textDocument: { uri: },
        position: pos,
      }
      text = @server.open_texts[uri]
      unless text
        respond(nil)
        return
      end
      defs = @server.type_definitions(text.path, TypeProf::CodePosition.from_lsp(pos))
      if defs.empty?
        respond(nil)
      else
        respond(defs.map do |path, code_range|
          {
            uri: @server.path_to_uri(path),
            range: code_range.to_lsp,
          }
        end)
      end
    end
  end

  class Message::TextDocument::References < Message
    METHOD = "textDocument/references" # request
    def run
      @params => {
        textDocument: { uri: },
        position: pos,
      }
      text = @server.open_texts[uri]
      unless text
        respond(nil)
        return
      end
      callsites = @server.references(text.path, TypeProf::CodePosition.from_lsp(pos))
      if callsites
        respond(callsites.map do |path, code_range|
          {
            uri: @server.path_to_uri(path),
            range: code_range.to_lsp,
          }
        end)
      else
        respond(nil)
      end
    end
  end

  class Message::TextDocument::Hover < Message
    METHOD = "textDocument/hover" # request
    def run
      @params => {
        textDocument: { uri: },
        position: pos,
      }
      text = @server.open_texts[uri]
      unless text
        respond(nil)
        return
      end
      str = @server.hover(text.path, TypeProf::CodePosition.from_lsp(pos))
      if str
        respond(contents: { language: "ruby", value: str })
      else
        respond(nil)
      end
    end
  end

  class Message::TextDocument::CodeLens < Message
    METHOD = "textDocument/codeLens"
    def run
      @params => { textDocument: { uri: } }
      text = @server.open_texts[uri]
      if !text || !@server.signature_enabled
        respond(nil)
        return
      end
      ret = []
      @server.code_lens(text.path) do |code_range, title|
        pos = code_range.first
        ret << {
          range: TypeProf::CodeRange.new(pos, pos.right).to_lsp,
          command: {
            title: "#: " + title,
            command: "typeprof.createPrototypeRBS",
            arguments: [uri, code_range.first.lineno, code_range.first.column, title],
          },
        }
      end
      respond(ret)
    end
  end

  # textDocument/documentSymbol request

  # textDocument/diagnostic request
  # workspace/diagnostic request
  #   workspace/diagnostic/refresh request

  class Message::TextDocument::Completion < Message
    METHOD = "textDocument/completion" # request
    def run
      @params => {
        textDocument: { uri: },
        position: pos,
      }
      #trigger_kind = @params.key?(:context) ? @params[:context][:triggerKind] : 1 # Invoked
      text = @server.open_texts[uri]
      unless text
        respond(nil)
        return
      end
      items = []
      sort = "aaaa"
      text.modify_for_completion(text, pos) do |string, trigger, pos|
        @server.update_file(text.path, string)
        pos = TypeProf::CodePosition.from_lsp(pos)
        @server.completion(text.path, trigger, pos) do |mid, hint|
          items << {
            label: mid,
            kind: 2, # Method
            sortText: sort,
            detail: hint,
          }
          sort = sort.succ
        end
      end
      respond(
        isIncomplete: false,
        items: items,
      )
      @server.update_file(text.path, text.string)
    end
  end

  # textDocument/signatureHelp request

  # textDocument/prepareRename request

  class Message::TextDocument::Rename < Message
    METHOD = "textDocument/rename" # request
    def run
      @params => {
        textDocument: { uri: },
        position: pos,
        newName:,
      }
      text = @server.open_texts[uri]
      unless text
        respond(nil)
        return
      end
      renames = @server.rename(text.path, TypeProf::CodePosition.from_lsp(pos))
      if renames
        changes = {}
        renames.each do |path, cr|
          (changes[@server.path_to_uri(path)] ||= []) << {
            range: cr.to_lsp,
            newText: newName,
          }
        end
        respond({
          changes:,
        })
      else
        respond(nil)
      end
    end
  end

  module Message::Workspace
  end

  # workspace/symbol request
  #   workspaceSymbol/resolve request

  # workspace/didChangeWatchedFiles notification

  class Message::Workspace::ExecuteCommand < Message
    METHOD = "workspace/executeCommand" # request
    def run
      case @params[:command]
      when "typeprof.enableSignature"
        @server.signature_enabled = true
        @server.send_request("workspace/codeLens/refresh")
        respond(nil)
      when "typeprof.disableSignature"
        @server.signature_enabled = false
        @server.send_request("workspace/codeLens/refresh")
        respond(nil)
      when "typeprof.createPrototypeRBS"
        uri, row, col, str = @params[:arguments]
        @server.send_request(
          "workspace/applyEdit",
          edit: {
            changes: {
              uri => [{
                range: TypeProf::CodeRange[row, col, row, col].to_lsp,
                newText: "#: #{ str }\n" + " " * col,
              }],
            },
          },
        ) do |res, err|
        end
        respond(nil)
      end
    end
  end

  Message.build_table
end
