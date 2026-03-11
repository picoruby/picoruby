module TypeProf::Core
  class Service
    def initialize(options)
      @options = options

      @rb_text_nodes = {}
      @rbs_text_nodes = {}

      @genv = GlobalEnv.new
      @genv.load_core_rbs(load_rbs_declarations(@options[:rbs_collection]).declarations)

      Builtin.new(genv).deploy
    end

    def load_rbs_declarations(rbs_collection)
      if rbs_collection
        loader = RBS::EnvironmentLoader.new
        loader.add_collection(rbs_collection)
        RBS::Environment.from_loader(loader)
      else
        return $raw_rbs_env if defined?($raw_rbs_env)
        loader = RBS::EnvironmentLoader.new
        $raw_rbs_env = RBS::Environment.from_loader(loader)
      end
    end

    attr_reader :genv

    def reset!
      @rb_text_nodes.each_value {|node| node.undefine(@genv) }
      @rbs_text_nodes.each_value {|nodes| nodes.each {|n| n.undefine(@genv) } }
      @genv.define_all
      @rb_text_nodes.each_value {|node| node.uninstall(@genv) }
      @rbs_text_nodes.each_value {|nodes| nodes.each {|n| n.uninstall(@genv) } }
      @genv.run_all
      @rb_text_nodes.clear
      @rbs_text_nodes.clear
    end

    def add_workspace(rb_folder, rbs_folder)
      Dir.glob(File.expand_path(rb_folder + "/**/*.{rb,rbs}")) do |path|
        update_file(path, nil)
      end
      Dir.glob(File.expand_path(rbs_folder + "/**/*.{rb,rbs}")) do |path|
        update_file(path, nil)
      end
    end

    def update_file(path, code)
      if File.extname(path) == ".rbs"
        update_rbs_file(path, code)
      else
        update_rb_file(path, code)
      end
    end

    def update_rb_file(path, code)
      prev_node = @rb_text_nodes[path]

      code = File.read(path) unless code
      node = AST.parse_rb(path, code)
      return false unless node

      node.diff(@rb_text_nodes[path]) if prev_node
      @rb_text_nodes[path] = node

      node.define(@genv)
      prev_node.undefine(@genv) if prev_node
      @genv.define_all

      node.install(@genv)
      prev_node.uninstall(@genv) if prev_node
      @genv.run_all

      # invariant validation
      if prev_node
        live_vtxs = []
        node.get_vertexes(live_vtxs)
        set = Set[]
        live_vtxs.uniq.each {|vtx| set << vtx }
        live_vtxs = set

        dead_vtxs = []
        prev_node.get_vertexes(dead_vtxs)
        set = Set[]
        dead_vtxs.uniq.each {|vtx| set << vtx }
        dead_vtxs = set

        live_vtxs.each do |vtx|
          next unless vtx
          raise vtx.inspect if dead_vtxs.include?(vtx)
        end

        global_vtxs = []
        @genv.get_vertexes(global_vtxs)
        set = Set[]
        global_vtxs.uniq.each {|vtx| set << vtx }
        global_vtxs = set

        global_vtxs.each do |global_vtx|
          next unless global_vtx.is_a?(Vertex)
          raise if dead_vtxs.include?(global_vtx)
          global_vtx.types.each_value do |prev_vtxs|
            prev_vtxs.each do |prev_vtx|
              raise if dead_vtxs.include?(prev_vtx)
            end
          end
          global_vtx.next_vtxs.each do |next_vtx|
            raise "#{ next_vtx }" if dead_vtxs.include?(next_vtx)
          end
        end
      end

      return true
    end

    def update_rbs_file(path, code)
      prev_decls = @rbs_text_nodes[path]

      code = File.read(path) unless code
      begin
        decls = AST.parse_rbs(path, code)
      rescue RBS::ParsingError
        return false
      end

      # TODO: diff
      @rbs_text_nodes[path] = decls

      decls.each {|decl| decl.define(@genv) }
      prev_decls.each {|decl| decl.undefine(@genv) } if prev_decls
      @genv.define_all

      decls.each {|decl| decl.install(@genv) }
      prev_decls.each {|decl| decl.uninstall(@genv) } if prev_decls
      @genv.run_all

      true
    end

    def process_diagnostic_paths(&blk)
      @genv.process_diagnostic_paths(&blk)
    end

    def diagnostics(path, &blk)
      @rb_text_nodes[path]&.each_diagnostic(@genv, &blk)
    end

    def definitions(path, pos)
      defs = []
      @rb_text_nodes[path]&.retrieve_at(pos) do |node|
        node.boxes(:cread) do |box|
          if box.const_read && box.const_read.cdef
            box.const_read.cdef.defs.each do |cdef_node|
              defs << [cdef_node.lenv.path, cdef_node.cname_code_range]
            end
          end
        end
        node.boxes(:mcall) do |box|
          boxes = []
          box.changes.boxes.each do |key, box|
            # ad-hocly handle Class#new calls
            if key[0] == :mcall && key[3] == :initialize # XXX: better condition?
              boxes << box
            end
          end
          boxes << box if boxes.empty?
          boxes.each do |box|
            box.resolve(genv, nil) do |me, _ty, mid, _orig_ty|
              next unless me
              me.decls.each do |mdecl|
                next unless mdecl.node.lenv.path

                code_range =
                  if mdecl.node.respond_to?(:mname_code_range)
                    mdecl.node.mname_code_range(mid)
                  else
                    mdecl.node.code_range
                  end

                defs << [mdecl.node.lenv.path, code_range]
              end
              me.defs.each do |mdef|
                code_range =
                  if mdef.node.respond_to?(:mname_code_range)
                    mdef.node.mname_code_range(mid)
                  else
                    mdef.node.code_range
                  end

                defs << [mdef.node.lenv.path, code_range]
              end
            end
          end
        end
        return defs unless defs.empty?
      end
      return defs
    end

    def type_definitions(path, pos)
      @rb_text_nodes[path]&.retrieve_at(pos) do |node|
        if node.ret
          ty_defs = []
          node.ret.types.map do |ty, _source|
            if ty.is_a?(Type::Instance)
              ty.mod.module_decls.each do |mdecl|
                # TODO
              end
              ty.mod.module_defs.each do |mdef_node|
                ty_defs << [mdef_node.lenv.path, mdef_node.code_range]
              end
            end
          end
          return ty_defs
        end
      end
      []
    end

    #: (String, TypeProf::CodePosition) -> Array[[String?, TypeProf::CodeRange]]?
    def references(path, pos)
      refs = []
      @rb_text_nodes[path]&.retrieve_at(pos) do |node|
        case node
        when AST::DefNode
          if node.mid_code_range.include?(pos)
            node.boxes(:mdef) do |mdef|
              me = @genv.resolve_method(mdef.cpath, mdef.singleton, mdef.mid)
              if me
                me.method_call_boxes.each do |box|
                  node = box.node
                  refs << [node.lenv.path, node.code_range]
                end
              end
            end
          end
        # TODO: Callsite
        when AST::ConstantReadNode
          if node.cname_code_range.include?(pos)
            node.boxes(:cread) do |cread_box|
              @genv.resolve_const(cread_box.const_read.cpath).read_boxes.each do |box|
                node = box.node
                refs << [node.lenv.path, node.code_range]
              end
            end
          end
        when AST::ConstantWriteNode
          if node.cname_code_range && node.cname_code_range.include?(pos) && node.static_cpath
            @genv.resolve_const(node.static_cpath).read_boxes.each do |box|
              node = box.node
              refs << [node.lenv.path, node.code_range]
            end
          end
        end
      end
      refs = refs.uniq
      return refs.empty? ? nil : refs
    end

    def rename(path, pos)
      mdefs = []
      cdefs = []
      @rb_text_nodes[path]&.retrieve_at(pos) do |node|
        node.boxes(:mcall) do |box|
          box.resolve(genv, nil) do |me, _ty, _mid, _orig_ty|
            next unless me
            me.defs.each do |mdef|
              mdefs << mdef
            end
          end
        end
        node.boxes(:cread) do |box|
          if box.node.cname_code_range.include?(pos)
            box.const_read.cdef.defs.each do |cdef|
              cdefs << cdef
            end
          end
        end
        if node.is_a?(AST::ConstantWriteNode)
          if node.cname_code_range.include?(pos) && node.static_cpath
            genv.resolve_const(node.static_cpath).defs.each do |cdef|
              cdefs << cdef
            end
          end
        end
        if node.is_a?(AST::DefNode) && node.mid_code_range.include?(pos)
          node.boxes(:mdef) do |mdef|
            mdefs << mdef
          end
        end
      end
      targets = []
      mdefs.each do |mdef|
        # TODO: support all method definition nodes rather than defn/defs (e.g., attr_reader, alias, SIG_DEF, etc.)
        targets << [mdef.node.lenv.path, mdef.node.mid_code_range]
        me = @genv.resolve_method(mdef.cpath, mdef.singleton, mdef.mid)
        if me
          me.method_call_boxes.each do |box|
            # TODO: if it is a super node, we need to change its method name too
            targets << [box.node.lenv.path, box.node.mid_code_range]
          end
        end
      end
      cdefs.each do |cdef|
        if cdef.is_a?(AST::ConstantWriteNode)
          targets << [cdef.lenv.path, cdef.cname_code_range] if cdef.cname_code_range
        end
        ve = @genv.resolve_const(cdef.static_cpath)
        ve.read_boxes.each do |box|
          targets << [box.node.lenv.path, box.node.cname_code_range]
        end
      end
      if targets.all? {|_path, cr| cr }
        targets.uniq
      else
        # TODO: report an error
        nil
      end
    end

    def hover(path, pos)
      @rb_text_nodes[path]&.retrieve_at(pos) do |node|
        node.boxes(:mcall) do |box|
          boxes = []
          box.changes.boxes.each do |key, box|
            # ad-hocly handle Class#new calls
            if key[0] == :mcall && key[3] == :initialize # XXX: better condition?
              boxes << box
            end
          end
          boxes << box if boxes.empty?
          boxes.each do |box|
            box.resolve(genv, nil) do |me, ty, mid, orig_ty|
              if me
                if !me.decls.empty?
                  me.decls.each do |mdecl|
                    return "#{ orig_ty.show }##{ mid } : #{ mdecl.show }"
                  end
                end
                if !me.defs.empty?
                  me.defs.each do |mdef|
                    return "#{ orig_ty.show }##{ mid } : #{ mdef.show(@options[:output_parameter_names]) }"
                  end
                end
              end
            end
          end
          return "??? failed to hover"
        end
        return node.ret ? node.ret.show : "??? no type ???"
      end
    end

    def code_lens(path)
      cpaths = []
      @rb_text_nodes[path]&.traverse do |event, node|
        if node.is_a?(AST::ModuleBaseNode)
          if node.static_cpath
            if event == :enter
              cpaths << node.static_cpath
            else
              cpaths.pop
            end
          end
        else
          if event == :enter
            next if node.is_a?(AST::DefNode) && node.rbs_method_type
            node.boxes(:mdef) do |mdef|
              hint = mdef.show(@options[:output_parameter_names])
              if hint
                yield mdef.node.code_range, hint
              end
            end
          end
        end
      end
    end

    def completion(path, trigger, pos)
      @rb_text_nodes[path]&.retrieve_at(pos) do |node|
        if node.code_range.last == pos.right
          node.ret.types.map do |ty, _source|
            base_ty = ty.base_type(genv)

            @genv.each_superclass(base_ty.mod, base_ty.is_a?(Type::Singleton)) do |mod, singleton|
              mod.methods[singleton].each do |mid, me|
                sig = nil
                me.decls.each do |mdecl|
                  sig = mdecl.method_types.map {|method_type| method_type.instance_variable_get(:@raw_node).to_s }.join(" | ")
                  break
                end
                unless sig
                  me.defs.each do |mdef|
                    sig = mdef.show(@options[:output_parameter_names])
                    break
                  end
                end
                yield mid, "#{ mod.cpath.join("::" )}#{ singleton ? "." : "#" }#{ mid } : #{ sig }" if sig
              end
            end
          end
          return
        end
      end
    end

    def dump_declarations(path)
      stack = []
      out = []
      @rb_text_nodes[path]&.traverse do |event, node|
        case node
        when AST::ModuleNode
          if node.static_cpath
            if event == :enter
              out << "  " * stack.size + "module #{ node.static_cpath.join("::") }"
              if stack == [:toplevel]
                out << "end"
                stack.pop
              end
              stack.push(node)
            else
              stack.pop
              out << "  " * stack.size + "end"
            end
          end
        when AST::ClassNode, AST::SingletonClassNode
          if node.static_cpath
            next if stack.any? { node.is_a?(AST::SingletonClassNode) && (_1.is_a?(AST::ClassNode) || _1.is_a?(AST::ModuleNode)) && node.static_cpath == _1.static_cpath }

            if event == :enter
              s = "class #{ node.static_cpath.join("::") }"
              mod = @genv.resolve_cpath(node.static_cpath)
              superclass = mod.superclass
              if superclass == nil
                s << " # failed to identify its superclass"
              elsif superclass.cpath != []
                s << " < #{ superclass.show_cpath }"
              end
              if stack == [:toplevel]
                out << "end"
                stack.pop
              end
              out << "  " * stack.size + s
              stack.push(node)
              mod.included_modules.each do |inc_def, inc_mod|
                if inc_def.is_a?(AST::ConstantReadNode) && inc_def.lenv.path == path
                  out << "  " * stack.size + "include #{ inc_mod.show_cpath }"
                end
              end
            else
              stack.pop
              out << "  " * stack.size + "end"
            end
          end
        when AST::ConstantWriteNode
          if node.static_cpath
            if event == :enter
              out << "  " * stack.size + "#{ node.static_cpath.join("::") }: #{ node.ret.show }"
            end
          end
        else
          if event == :enter
            node.boxes(:mdef) do |mdef|
              if stack.empty?
                out << "  " * stack.size + "class Object"
                stack << :toplevel
              end
              if @options[:output_source_locations]
                pos = mdef.node.code_range.first
                out << "  " * stack.size + "# #{ path }:#{ pos.lineno }:#{ pos.column + 1 }"
              end
              out << "  " * stack.size + "def #{ mdef.singleton ? "self." : "" }#{ mdef.mid }: " + mdef.show(@options[:output_parameter_names])
            end
          end
        end
      end
      if stack == [:toplevel]
        out << "end"
        stack.pop
      end
      out.join("\n") + "\n"
    end

    def get_method_sig(cpath, singleton, mid)
      s = []
      @genv.resolve_method(cpath, singleton, mid).defs.each do |mdef|
        s << "def #{ mid }: " + mdef.show
      end
      s
    end

    def batch(files, output)
      if @options[:output_typeprof_version]
        output.puts "# TypeProf #{ TypeProf::VERSION }"
        output.puts
      end

      i = 0
      show_files = files.select do |file|
        if @options[:display_indicator]
          $stderr << "\r[%d/%d] %s\e[K" % [i, files.size, file]
          i += 1
        end

        res = update_file(file, File.read(file))

        if res
          true
        else
          output.puts "# failed to analyze: #{ file }"
          false
        end
      rescue => e
        output.puts "# error: #{ file }"
        raise e
      end
      if @options[:display_indicator]
        $stderr << "\r\e[K"
      end

      first = true
      show_files.each do |file|
        next if File.extname(file) == ".rbs"
        output.puts unless first
        first = false
        output.puts "# #{ file }"
        if @options[:output_diagnostics]
          diagnostics(file) do |diag|
            output.puts "# #{ diag.code_range.to_s }:#{ diag.msg }"
          end
        end
        output.puts dump_declarations(file)
      end
    end
  end
end

if $0 == __FILE__
  core = TypeProf::Core::Service.new({})
  core.add_workspaces(["foo"].to_a)
  core.update_rb_file("foo", "foo")
end
