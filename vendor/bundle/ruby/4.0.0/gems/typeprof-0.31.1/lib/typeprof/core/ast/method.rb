module TypeProf::Core
  class AST
    def self.get_rbs_comment_before(raw_node, lenv)
      comments = Fiber[:comments]
      i = comments.bsearch_index {|comment| comment.location.start_line >= raw_node.location.start_line } || comments.size
      lineno = raw_node.location.start_line
      rbs_comments = []
      while i > 0
        i -= 1
        lineno -= 1
        comment = comments[i]
        comment_loc = comment.location
        comment_text = comment_loc.slice
        if comment_loc.start_line == lineno && comment_text.start_with?("#:")
          rbs_comments[comment_loc.start_line] = " " * (comment_loc.start_column + 2) + comment_text[2..]
        else
          break
        end
      end
      return nil if rbs_comments.empty?
      rbs_comments = rbs_comments.map {|line| line || "" }.join("\n")
      method_type = RBS::Parser.parse_method_type(rbs_comments)
      if method_type
        AST.create_rbs_func_type(method_type, method_type.type_params, method_type.block, lenv)
      else
        nil
      end
    rescue RBS::ParsingError
      # TODO: report the error
      nil
    end

    def self.parse_params(tbl, raw_args, lenv)
      unless raw_args
        return {
          req_positionals: [],
          opt_positionals: [],
          opt_positional_defaults: [],
          rest_positionals: nil,
          post_positionals: [],
          req_keywords: [],
          opt_keywords: [],
          opt_keyword_defaults: [],
          rest_keywords: nil,
          block: nil,
        }
      end

      args_code_ranges = []
      req_positionals = []
      raw_args.requireds.each do |n|
        args_code_ranges << TypeProf::CodeRange.from_node(n.location)
        req_positionals << (n.is_a?(Prism::MultiTargetNode) ? nil : n.name)
      end

      # pre_init = args[1]

      opt_positionals = []
      opt_positional_defaults = []
      raw_args.optionals.each do |n|
        opt_positionals << n.name
        opt_positional_defaults << AST.create_node(n.value, lenv)
      end

      post_positionals = raw_args.posts.map {|n| (n.is_a?(Prism::MultiTargetNode) ? nil : n.name) }

      rest_positionals = raw_args.rest ? (raw_args.rest.name || :"*anonymous_rest") : nil

      req_keywords = []
      opt_keywords = []
      opt_keyword_defaults = []

      raw_args.keywords.each do |kw|
        case kw.type
        when :required_keyword_parameter_node
          req_keywords << kw.name
        when :optional_keyword_parameter_node
          opt_keywords << kw.name
          opt_keyword_defaults << AST.create_node(kw.value, lenv)
        end
      end

      case raw_args.keyword_rest
      when Prism::KeywordRestParameterNode
        rest_keywords = raw_args.keyword_rest.name || :"**anonymous_keyword"
      when Prism::ForwardingParameterNode
        # TODO: The variable names might be subject to change when supporting the propagation of parameter values to the graph during method calls.
        rest_positionals = :"..."
        rest_keywords = :"..."
      when Prism::NoKeywordsParameterNode
        # what to do?
      when nil
        # nothing to do
      else
        raise "unexpected keyword rest: #{ raw_args.keyword_rest.class }"
      end

      block = raw_args.block.name || :"*anonymous_block" if raw_args.block

      {
        req_positionals:,
        opt_positionals:,
        opt_positional_defaults:,
        rest_positionals:,
        post_positionals:,
        req_keywords:,
        opt_keywords:,
        opt_keyword_defaults:,
        rest_keywords:,
        block:,
        args_code_ranges:
      }
    end

    class DefNode < Node
      def initialize(raw_node, lenv, use_result)
        super(raw_node, lenv)
        # TODO: warn "def self.foo" in a metaclass
        singleton = !!raw_node.receiver || lenv.cref.scope_level == :metaclass
        mid = raw_node.name
        mid_code_range = TypeProf::CodeRange.from_node(raw_node.name_loc)
        @tbl = raw_node.locals
        raw_args = raw_node.parameters
        raw_body = raw_node.body

        @rbs_method_type = AST.get_rbs_comment_before(raw_node, lenv)

        @singleton = singleton
        @mid = mid
        @mid_code_range = mid_code_range

        ncref = CRef.new(lenv.cref.cpath, @singleton ? :class : :instance, @mid, lenv.cref)
        nlenv = LocalEnv.new(@lenv.path, ncref, {}, [])
        if raw_body
          @body = AST.create_node(raw_body, nlenv)
        else
          pos = code_range.last.left.left.left # before "end"
          cr = TypeProf::CodeRange.new(pos, pos)
          @body = DummyNilNode.new(cr, nlenv)
        end

        h = AST.parse_params(@tbl, raw_args, nlenv)
        @req_positionals = h[:req_positionals]
        @opt_positionals = h[:opt_positionals]
        @opt_positional_defaults = h[:opt_positional_defaults]
        @rest_positionals = h[:rest_positionals]
        @post_positionals = h[:post_positionals]
        @req_keywords = h[:req_keywords]
        @opt_keywords = h[:opt_keywords]
        @opt_keyword_defaults = h[:opt_keyword_defaults]
        @rest_keywords = h[:rest_keywords]
        @block = h[:block]
        @args_code_ranges = h[:args_code_ranges] || []

        # If the result of `def` statement, stop reusing this node
        # TODO: `private def ...` should be handled well
        @reusable = !use_result
      end

      attr_reader :singleton, :mid, :mid_code_range
      attr_reader :tbl
      attr_reader :req_positionals
      attr_reader :opt_positionals
      attr_reader :opt_positional_defaults
      attr_reader :rest_positionals
      attr_reader :post_positionals
      attr_reader :req_keywords
      attr_reader :opt_keywords
      attr_reader :opt_keyword_defaults
      attr_reader :rest_keywords
      attr_reader :block
      attr_reader :body
      attr_reader :rbs_method_type
      attr_reader :reusable

      def subnodes = {
        body:,
        opt_positional_defaults:,
        opt_keyword_defaults:,
        rbs_method_type:,
      }
      def attrs = {
        singleton:,
        mid:,
        mid_code_range:,
        tbl:,
        req_positionals:,
        opt_positionals:,
        rest_positionals:,
        post_positionals:,
        req_keywords:,
        opt_keywords:,
        rest_keywords:,
        block:,
        reusable:,
      }

      def mname_code_range(_name) = @mid_code_range

      def define(genv) # NOT define0
        return define_copy(genv) if @prev_node && @reusable
        super(genv)
      end

      def install(genv) # NOT install0
        return install_copy(genv) if @prev_node && @reusable
        super(genv)
      end

      def install0(genv)
        if @rbs_method_type
          @changes.add_method_decl_box(genv, @lenv.cref.cpath, @singleton, @mid, [@rbs_method_type], false)
        end

        @tbl.each {|var| @body.lenv.locals[var] = Source.new(genv.nil_type) }
        @body.lenv.locals[:"*self"] = @body.lenv.cref.get_self(genv)

        req_positionals = @req_positionals.map {|var| @body.lenv.new_var(var, self) }
        opt_positionals = @opt_positionals.map {|var| @body.lenv.new_var(var, self) }
        rest_positionals = @rest_positionals ? @body.lenv.new_var(@rest_positionals, self) : nil
        post_positionals = @post_positionals.map {|var| @body.lenv.new_var(var, self) }
        req_keywords = @req_keywords.map {|var| @body.lenv.new_var(var, self) }
        opt_keywords = @opt_keywords.map {|var| @body.lenv.new_var(var, self) }
        rest_keywords = @rest_keywords ? @body.lenv.new_var(@rest_keywords, self) : nil
        block = @block ? @body.lenv.new_var(@block, self) : nil

        if rest_positionals
          @changes.add_edge(genv, Source.new(genv.gen_ary_type(Vertex.new(self))), rest_positionals)
        end

        if rest_keywords
          if @rest_keywords == :"**anonymous_keyword"
            @changes.add_edge(genv, Source.new(genv.gen_hash_type(Vertex.new(self), Vertex.new(self))), rest_keywords)
          end
        end

        @opt_positional_defaults.zip(opt_positionals) do |expr, vtx|
          @changes.add_edge(genv, expr.install(genv), vtx)
        end
        @opt_keyword_defaults.zip(opt_keywords) do |expr, vtx|
          @changes.add_edge(genv, expr.install(genv), vtx)
        end

        if block
          block = @body.lenv.set_var(:"*given_block", block)
        else
          block = @body.lenv.new_var(:"*given_block", self)
        end

        if @body
          @body.lenv.locals[:"*expected_method_ret"] = Vertex.new(self)
          @body.install(genv)
          @body.lenv.add_return_box(@changes.add_escape_box(genv, @body.ret))
        end

        f_args = FormalArguments.new(
          req_positionals,
          opt_positionals,
          rest_positionals,
          post_positionals,
          req_keywords,
          opt_keywords,
          rest_keywords,
          block,
        )

        @changes.add_method_def_box(genv, @lenv.cref.cpath, @singleton, @mid, f_args, @body.lenv.return_boxes)

        Source.new(Type::Symbol.new(genv, @mid))
      end

      def last_stmt_code_range
        if @body
          if @body.is_a?(AST::StatementsNode)
            @body.stmts.last.code_range
          else
            @body.code_range
          end
        else
          nil
        end
      end

      def retrieve_at(pos, &blk)
        if @rbs_method_type
          if @rbs_method_type.code_range.include?(pos) # annotation
            @rbs_method_type.retrieve_at(pos, &blk)
          end
        end
        @args_code_ranges.each_with_index do |cr, i|
          if cr.include?(pos)
            yield DummySymbolNode.new(@tbl[i], cr, @body.lenv.get_var(@tbl[i]))
            break
          end
        end
        super(pos, &blk)
      end

      def modified_vars(tbl, vars)
        # skip
      end
    end

    class AliasNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @new_mid = AST.create_node(raw_node.new_name, lenv)
        @old_mid = AST.create_node(raw_node.old_name, lenv)
      end

      attr_reader :new_mid, :old_mid

      def subnodes = { new_mid:, old_mid: }

      def install0(genv)
        @new_mid.install(genv)
        @old_mid.install(genv)
        if @new_mid.is_a?(SymbolNode) && @old_mid.is_a?(SymbolNode)
          new_mid = @new_mid.lit
          old_mid = @old_mid.lit
          box = @changes.add_method_alias_box(genv, @lenv.cref.cpath, false, new_mid, old_mid)
          box.ret
        else
          Source.new(genv.nil_type)
        end
      end
    end

    class UndefNode < Node
      def initialize(raw_node, lenv)
        super(raw_node, lenv)
        @names = raw_node.names.map do |raw_name|
          AST.create_node(raw_name, lenv)
        end
      end

      attr_reader :names

      def subnodes = { names: }

      def install0(genv)
        @names.each do |name|
          name.install(genv)
        end
        Source.new(genv.nil_type)
      end
    end
  end
end
