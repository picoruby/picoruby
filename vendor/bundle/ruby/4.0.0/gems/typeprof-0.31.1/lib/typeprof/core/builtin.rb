module TypeProf::Core
  class Builtin
    def initialize(genv)
      @genv = genv
    end

    def class_new(changes, node, ty, a_args, ret)
      type_param_vtxs = ty.mod.type_params.map { Vertex.new(node) }
      temp_instance_ty = Type::Instance.new(@genv, ty.mod, type_param_vtxs)
      recv = Source.new(temp_instance_ty)
      changes.add_method_call_box(@genv, recv, :initialize, a_args, false)
      changes.add_edge(@genv, recv, ret)
      true
    end

    def object_class(changes, node, ty, a_args, ret)
      ty = ty.base_type(@genv)
      mod = ty.is_a?(Type::Instance) ? ty.mod : @genv.mod_class
      ty = Type::Singleton.new(@genv, mod)
      vtx = Source.new(ty)
      changes.add_edge(@genv, vtx, ret)
      true
    end

    def proc_call(changes, node, ty, a_args, ret)
      case ty
      when Type::Proc
        ty.block.accept_args(@genv, changes, a_args.positionals)
        ty.block.add_ret(@genv, changes, ret)
        true
      else
        false
      end
    end

    def array_aref(changes, node, ty, a_args, ret)
      if a_args.positionals.size == 1
        case ty
        when Type::Array
          idx = node.positional_args[0]
          if idx.is_a?(AST::IntegerNode)
            idx = idx.lit
          else
            idx = nil
          end
          vtx = ty.get_elem(@genv, idx)
          changes.add_edge(@genv, vtx, ret)
          true
        else
          false
        end
      else
        false
      end
    end

    def array_aset(changes, node, ty, a_args, ret)
      if a_args.positionals.size == 2
        case ty
        when Type::Array
          val = a_args.positionals[1]
          idx = node.positional_args[0]
          if idx.is_a?(AST::IntegerNode) && ty.get_elem(@genv, idx.lit)
            changes.add_edge(@genv, val, ty.get_elem(@genv, idx.lit))
          else
            changes.add_edge(@genv, val, ty.get_elem(@genv))
          end
          true
        else
          false
        end
      else
        false
      end
    end

    def array_push(changes, node, ty, a_args, ret)
      if a_args.positionals.size == 1
        if ty.is_a?(Type::Array)
          val = a_args.positionals[0]
          changes.add_edge(@genv, val, ty.get_elem(@genv))
        end
        recv = Source.new(ty)
        changes.add_edge(@genv, recv, ret)
      else
        false
      end
    end

    def hash_aref(changes, node, ty, a_args, ret)
      if a_args.positionals.size == 1
        case ty
        when Type::Hash, Type::Record
          idx = node.positional_args[0]
          idx = idx.is_a?(AST::SymbolNode) ? idx.lit : nil
          value = ty.get_value(idx)
          if value
            changes.add_edge(@genv, value, ret)
          else
            # Return untyped for unknown fields
            changes.add_edge(@genv, Source.new(), ret)
          end
          true
        else
          false
        end
      else
        false
      end
    end

    def hash_aset(changes, node, ty, a_args, ret)
      if a_args.positionals.size == 2
        case ty
        when Type::Hash
          val = a_args.positionals[1]
          idx = node.positional_args[0]
          if idx.is_a?(AST::SymbolNode) && ty.get_value(idx.lit)
            # TODO: how to handle new key?
            changes.add_edge(@genv, val, ty.get_value(idx.lit))
          else
            # TODO: literal_pairs will not be updated
            changes.add_edge(@genv, a_args.positionals[0], ty.get_key)
            changes.add_edge(@genv, val, ty.get_value)
          end
          changes.add_edge(@genv, val, ret)
          true
        else
          false
        end
      else
        false
      end
    end

    def deploy
      {
        class_new: [[:Class], false, :new],
        object_class: [[:Object], false, :class],
        proc_call: [[:Proc], false, :call],
        array_aref: [[:Array], false, :[]],
        array_aset: [[:Array], false, :[]=],
        array_push: [[:Array], false, :<<],
        hash_aref: [[:Hash], false, :[]],
        hash_aset: [[:Hash], false, :[]=],
      }.each do |key, (cpath, singleton, mid)|
        me = @genv.resolve_method(cpath, singleton, mid)
        me.builtin = method(key)
      end
    end
  end
end
