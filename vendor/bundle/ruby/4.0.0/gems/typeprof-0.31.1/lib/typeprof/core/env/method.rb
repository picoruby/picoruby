module TypeProf::Core
  class FormalArguments
    def initialize(req_positionals, opt_positionals, rest_positionals, post_positionals, req_keywords, opt_keywords, rest_keywords, block)
      @req_positionals = req_positionals
      @opt_positionals = opt_positionals
      @rest_positionals = rest_positionals
      @post_positionals = post_positionals
      @req_keywords = req_keywords
      @opt_keywords = opt_keywords
      @rest_keywords = rest_keywords
      @block = block
    end

    Empty = FormalArguments.new([], [], nil, [], [], [], nil, nil)

    attr_reader :req_positionals
    attr_reader :opt_positionals
    attr_reader :rest_positionals
    attr_reader :post_positionals
    attr_reader :req_keywords
    attr_reader :opt_keywords
    attr_reader :rest_keywords
    attr_reader :block
  end

  class ActualArguments
    def initialize(positionals, splat_flags, keywords, block)
      @positionals = positionals
      @splat_flags = splat_flags
      @keywords = keywords
      @block = block
    end

    attr_reader :positionals, :splat_flags, :keywords, :block

    def new_vertexes(genv, node)
      positionals = @positionals.map {|arg| arg.new_vertex(genv, node) }
      splat_flags = @splat_flags
      keywords = @keywords ? @keywords.new_vertex(genv, node) : nil
      block = @block ? @block.new_vertex(genv, node) : nil
      ActualArguments.new(positionals, splat_flags, keywords, block)
    end

    def get_rest_args(genv, changes, start_rest, end_rest)
      vtxs = []

      start_rest.upto(end_rest - 1) do |i|
        a_arg = @positionals[i]
        if @splat_flags[i]
          a_arg.each_type do |ty|
            ty = ty.base_type(genv)
            if ty.is_a?(Type::Instance) && ty.mod == genv.mod_ary && ty.args[0]
              vtxs << changes.new_vertex(genv, self, ty.args[0])
            else
              "???"
            end
          end
        else
          vtxs << a_arg
        end
      end

      vtxs.uniq
    end

    def get_keyword_arg(genv, changes, name)
      vtx = Vertex.new(self)
      @keywords.each_type do |ty|
        case ty
        when Type::Hash
          changes.add_edge(genv, ty.get_value(name), vtx)
        when Type::Instance
          if ty.mod == genv.mod_hash
            changes.add_edge(genv, ty.args[1], vtx)
          end
        else
          # what to do?
        end
      end
      vtx
    end
  end

  class Block
    #: (AST::CallBaseNode, Vertex, Array[Vertex], Array[EscapeBox]) -> void
    def initialize(node, f_ary_arg, f_args, next_boxes)
      @node = node
      @f_ary_arg = f_ary_arg
      @f_args = f_args
      @next_boxes = next_boxes
    end

    attr_reader :node, :f_args, :next_boxes

    def accept_args(genv, changes, caller_positionals)
      if caller_positionals.size == 1 && @f_args.size >= 2
        single_arg = caller_positionals[0]

        @f_args.each_with_index do |f_arg, i|
          elem_vtx = changes.add_splat_box(genv, single_arg, i).ret
          changes.add_edge(genv, elem_vtx, f_arg)
        end
      else
        caller_positionals.zip(@f_args) do |a_arg, f_arg|
          changes.add_edge(genv, a_arg, f_arg) if f_arg
        end
      end
    end

    def add_ret(genv, changes, ret)
      @next_boxes.each do |box|
        changes.add_edge(genv, box.a_ret, ret)
      end
    end
  end

  class RecordBlock
    def initialize(node)
      @node = node
      @used = false
      @f_args = []
      @ret = Vertex.new(node)
    end

    def get_f_arg(i)
      @f_args[i] ||= Vertex.new(@node)
    end

    attr_reader :node, :f_args, :ret, :used

    def accept_args(genv, changes, caller_positionals)
      @used = true
      caller_positionals.each_with_index do |a_arg, i|
        changes.add_edge(genv, a_arg.new_vertex(genv, @node), get_f_arg(i))
      end
    end

    def add_ret(genv, changes, ret)
      changes.add_edge(genv, ret, @ret)
    end
  end
end
