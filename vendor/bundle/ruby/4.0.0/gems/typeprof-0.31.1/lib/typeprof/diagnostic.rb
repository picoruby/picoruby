module TypeProf
  class Diagnostic
    def initialize(node, meth, msg, tags: nil)
      @node = node
      @meth = meth
      @msg = msg
      @tags = tags
    end

    def reuse(new_node)
      @node = new_node
    end

    attr_reader :node, :msg, :tags

    def code_range
      @node.send(@meth)
    end

    SEVERITY = { error: 1, warning: 2, info: 3, hint: 4 }
    TAG = { unnecessary: 1, deprecated: 2 }

    def to_lsp(severity: :error)
      json = {
        range: code_range.to_lsp,
        source: "TypeProf",
        message: @msg,
      }
      json[:severity] = SEVERITY[severity]
      json[:tags] = @tags.map {|tag| TAG[tag] } if @tags
      json
    end
  end
end
