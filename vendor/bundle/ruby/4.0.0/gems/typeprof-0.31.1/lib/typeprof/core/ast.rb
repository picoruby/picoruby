module TypeProf::Core
  class AST
    def self.parse_rb(path, src)
      result = Prism.parse(src)

      return nil unless result.errors.empty?

      # comments, errors, magic_comments
      raw_scope = result.value

      raise unless raw_scope.type == :program_node

      Fiber[:comments] = result.comments

      cref = CRef::Toplevel
      lenv = LocalEnv.new(path, cref, {}, [])

      ProgramNode.new(raw_scope, lenv)
    end

    #: (untyped, TypeProf::Core::LocalEnv, ?bool) -> TypeProf::Core::AST::Node
    def self.create_node(raw_node, lenv, use_result = true)
      while true
        case raw_node.type
        when :parentheses_node
          raw_node = raw_node.body
        when :implicit_node
          raw_node = raw_node.value
        else
          break
        end
      end

      case raw_node.type

      # definition
      when :statements_node then StatementsNode.new(raw_node, lenv, use_result)
      when :module_node then ModuleNode.new(raw_node, lenv, use_result)
      when :class_node then ClassNode.new(raw_node, lenv, use_result)
      when :singleton_class_node then SingletonClassNode.new(raw_node, lenv, use_result)
      when :def_node then DefNode.new(raw_node, lenv, use_result)
      when :alias_method_node then AliasNode.new(raw_node, lenv)
      when :undef_node then UndefNode.new(raw_node, lenv)

      # control
      when :and_node then AndNode.new(raw_node, lenv)
      when :or_node then OrNode.new(raw_node, lenv)
      when :if_node then IfNode.new(raw_node, lenv)
      when :unless_node then UnlessNode.new(raw_node, lenv)
      when :case_node then CaseNode.new(raw_node, lenv)
      when :case_match_node then CaseMatchNode.new(raw_node, lenv)
      when :while_node then WhileNode.new(raw_node, lenv)
      when :until_node then UntilNode.new(raw_node, lenv)
      when :break_node then BreakNode.new(raw_node, lenv)
      when :next_node then NextNode.new(raw_node, lenv)
      when :redo_node then RedoNode.new(raw_node, lenv)
      when :return_node then ReturnNode.new(raw_node, lenv)
      when :begin_node then BeginNode.new(raw_node, lenv)
      when :retry_node then RetryNode.new(raw_node, lenv)
      when :rescue_modifier_node then RescueModifierNode.new(raw_node, lenv)
      when :rescue_node then RescueNode.new(raw_node, lenv)

      # constants
      when :constant_read_node, :constant_path_node
        ConstantReadNode.new(raw_node, lenv)
      when :constant_write_node, :constant_path_write_node
        ConstantWriteNode.new(raw_node, AST.create_node(raw_node.value, lenv), lenv)
      when :constant_operator_write_node
        read = ConstantReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        ConstantWriteNode.new(raw_node, rhs, lenv)
      when :constant_or_write_node
        read = ConstantReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        ConstantWriteNode.new(raw_node, rhs, lenv)
      when :constant_and_write_node
        read = ConstantReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        ConstantWriteNode.new(raw_node, rhs, lenv)
      when :constant_path_operator_write_node
        read = ConstantReadNode.new(raw_node.target, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        ConstantWriteNode.new(raw_node, rhs, lenv)
      when :constant_path_or_write_node
        read = ConstantReadNode.new(raw_node.target, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        ConstantWriteNode.new(raw_node, rhs, lenv)
      when :constant_path_and_write_node
        read = ConstantReadNode.new(raw_node.target, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        ConstantWriteNode.new(raw_node, rhs, lenv)

      # variables
      when :local_variable_read_node
        LocalVariableReadNode.new(raw_node, lenv)
      when :it_local_variable_read_node
        ItLocalVariableReadNode.new(raw_node, lenv)
      when :local_variable_write_node
        LocalVariableWriteNode.new(raw_node, AST.create_node(raw_node.value, lenv), lenv)
      when :local_variable_operator_write_node
        read = LocalVariableReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        LocalVariableWriteNode.new(raw_node, rhs, lenv)
      when :local_variable_or_write_node
        read = LocalVariableReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        LocalVariableWriteNode.new(raw_node, rhs, lenv)
      when :local_variable_and_write_node
        read = LocalVariableReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        LocalVariableWriteNode.new(raw_node, rhs, lenv)
      when :instance_variable_read_node
        InstanceVariableReadNode.new(raw_node, lenv)
      when :instance_variable_write_node
        InstanceVariableWriteNode.new(raw_node, AST.create_node(raw_node.value, lenv), lenv)
      when :instance_variable_operator_write_node
        read = InstanceVariableReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        InstanceVariableWriteNode.new(raw_node, rhs, lenv)
      when :instance_variable_or_write_node
        read = InstanceVariableReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        InstanceVariableWriteNode.new(raw_node, rhs, lenv)
      when :instance_variable_and_write_node
        read = InstanceVariableReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        InstanceVariableWriteNode.new(raw_node, rhs, lenv)
      when :class_variable_read_node
        ClassVariableReadNode.new(raw_node, lenv)
      when :class_variable_write_node
        ClassVariableWriteNode.new(raw_node, AST.create_node(raw_node.value, lenv), lenv)
      when :class_variable_operator_write_node
        read = ClassVariableReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
      when :class_variable_or_write_node
        read = ClassVariableReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        ClassVariableWriteNode.new(raw_node, rhs, lenv)
      when :class_variable_and_write_node
        read = ClassVariableReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        ClassVariableWriteNode.new(raw_node, rhs, lenv)
      when :global_variable_read_node
        GlobalVariableReadNode.new(raw_node, lenv)
      when :global_variable_write_node
        GlobalVariableWriteNode.new(raw_node, AST.create_node(raw_node.value, lenv), lenv)
      when :global_variable_operator_write_node
        read = GlobalVariableReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        GlobalVariableWriteNode.new(raw_node, rhs, lenv)
      when :global_variable_or_write_node
        read = GlobalVariableReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        GlobalVariableWriteNode.new(raw_node, rhs, lenv)
      when :global_variable_and_write_node
        read = GlobalVariableReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        GlobalVariableWriteNode.new(raw_node, rhs, lenv)
      when :numbered_reference_read_node
        RegexpReferenceReadNode.new(raw_node, lenv)
      when :back_reference_read_node
        RegexpReferenceReadNode.new(raw_node, lenv)

      # assignment targets
      when :index_operator_write_node
        read = IndexReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        IndexWriteNode.new(raw_node, rhs, lenv)
      when :index_or_write_node
        read = IndexReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        IndexWriteNode.new(raw_node, rhs, lenv)
      when :index_and_write_node
        read = IndexReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        IndexWriteNode.new(raw_node, rhs, lenv)
      when :call_operator_write_node
        read = CallReadNode.new(raw_node, lenv)
        rhs = OperatorNode.new(raw_node, read, lenv)
        CallWriteNode.new(raw_node, rhs, lenv)
      when :call_or_write_node
        read = CallReadNode.new(raw_node, lenv)
        rhs = OrNode.new(raw_node, read, raw_node.value, lenv)
        CallWriteNode.new(raw_node, rhs, lenv)
      when :call_and_write_node
        read = CallReadNode.new(raw_node, lenv)
        rhs = AndNode.new(raw_node, read, raw_node.value, lenv)
        CallWriteNode.new(raw_node, rhs, lenv)
      when :multi_write_node then MultiWriteNode.new(raw_node, lenv)
      when :match_write_node then MatchWriteNode.new(raw_node, lenv)

      # value
      when :self_node then SelfNode.new(raw_node, lenv)
      when :nil_node then NilNode.new(raw_node, lenv)
      when :true_node then TrueNode.new(raw_node, lenv)
      when :false_node then FalseNode.new(raw_node, lenv)
      when :integer_node then IntegerNode.new(raw_node, lenv)
      when :float_node then FloatNode.new(raw_node, lenv)
      when :rational_node then RationalNode.new(raw_node, lenv)
      when :imaginary_node then ComplexNode.new(raw_node, lenv)
      when :source_file_node then StringNode.new(raw_node, lenv, "")
      when :source_line_node then IntegerNode.new(raw_node, lenv, 0)
      when :source_encoding_node then SourceEncodingNode.new(raw_node, lenv)
      when :symbol_node then SymbolNode.new(raw_node, lenv)
      when :interpolated_symbol_node then InterpolatedSymbolNode.new(raw_node, lenv)
      when :string_node then StringNode.new(raw_node, lenv, raw_node.content)
      when :interpolated_string_node then InterpolatedStringNode.new(raw_node, lenv)
      when :x_string_node then StringNode.new(raw_node, lenv, "")
      when :interpolated_x_string_node then InterpolatedStringNode.new(raw_node, lenv)
      when :regular_expression_node then RegexpNode.new(raw_node, lenv)
      when :interpolated_regular_expression_node then InterpolatedRegexpNode.new(raw_node, lenv)
      when :match_last_line_node then MatchLastLineNode.new(raw_node, lenv)
      when :interpolated_match_last_line_node then InterpolatedMatchLastLineNode.new(raw_node, lenv)
      when :range_node then RangeNode.new(raw_node, lenv)
      when :array_node then ArrayNode.new(raw_node, lenv)
      when :hash_node then HashNode.new(raw_node, lenv, false)
      when :keyword_hash_node then HashNode.new(raw_node, lenv, true)
      when :lambda_node then LambdaNode.new(raw_node, lenv)

      # misc
      when :defined_node then DefinedNode.new(raw_node, lenv)
      when :splat_node then SplatNode.new(raw_node, lenv)
      when :for_node then ForNode.new(raw_node, lenv)
      when :alias_global_variable_node then AliasGlobalVariableNode.new(raw_node, lenv)
      when :post_execution_node then PostExecutionNode.new(raw_node, lenv)
      when :flip_flop_node then FlipFlopNode.new(raw_node, lenv)
      when :shareable_constant_node then create_node(raw_node.write, lenv)
      when :match_required_node then MatchRequiredNode.new(raw_node, lenv)
      when :match_predicate_node then MatchPredicateNode.new(raw_node, lenv)

      # call
      when :super_node then SuperNode.new(raw_node, lenv)
      when :forwarding_super_node then ForwardingSuperNode.new(raw_node, lenv)
      when :yield_node then YieldNode.new(raw_node, lenv)
      when :call_node
        if !raw_node.receiver
          # TODO: handle them only when it is directly under class or module
          case raw_node.name
          when :include
            return IncludeMetaNode.new(raw_node, lenv)
          when :attr_reader
            return AttrReaderMetaNode.new(raw_node, lenv)
          when :attr_accessor
            return AttrAccessorMetaNode.new(raw_node, lenv)
          end
        end
        CallNode.new(raw_node, lenv)
      else
        pp raw_node
        raise "not supported yet: #{ raw_node.type }"
      end
    end

    def self.create_target_node(raw_node, lenv)
      dummy_node = DummyRHSNode.new(TypeProf::CodeRange.from_node(raw_node.location), lenv)
      case raw_node.type
      when :local_variable_target_node
        LocalVariableWriteNode.new(raw_node, dummy_node, lenv)
      when :instance_variable_target_node
        InstanceVariableWriteNode.new(raw_node, dummy_node, lenv)
      when :class_variable_target_node
        ClassVariableWriteNode.new(raw_node, dummy_node, lenv)
      when :global_variable_target_node
        GlobalVariableWriteNode.new(raw_node, dummy_node, lenv)
      when :constant_target_node
        ConstantWriteNode.new(raw_node, dummy_node, lenv)
      when :constant_path_target_node
        ConstantWriteNode.new(raw_node, dummy_node, lenv)
      when :index_target_node
        IndexWriteNode.new(raw_node, dummy_node, lenv)
      when :call_target_node
        CallWriteNode.new(raw_node, dummy_node, lenv)
      when :multi_target_node
        MultiTargetNode.new(raw_node, dummy_node, lenv)
      else
        pp raw_node
        raise "not supported yet: #{ raw_node.type }"
      end
    end

    def self.create_pattern_node(raw_node, lenv)
      while true
        case raw_node.type
        when :parentheses_node
          raw_node = raw_node.body
        when :implicit_node
          raw_node = raw_node.value
        else
          break
        end
      end

      case raw_node.type
      when :array_pattern_node then ArrayPatternNode.new(raw_node, lenv)
      when :hash_pattern_node then HashPatternNode.new(raw_node, lenv)
      when :find_pattern_node then FindPatternNode.new(raw_node, lenv)

      when :alternation_pattern_node then AltPatternNode.new(raw_node, lenv)

      when :capture_pattern_node then CapturePatternNode.new(raw_node, lenv)

      when :if_node then IfPatternNode.new(raw_node, lenv)

      when :pinned_variable_node then PinnedPatternNode.new(raw_node, lenv)
      when :pinned_expression_node then PinnedPatternNode.new(raw_node, lenv)

      when :local_variable_target_node
        dummy_node = DummyRHSNode.new(TypeProf::CodeRange.from_node(raw_node.location), lenv)
        LocalVariableWriteNode.new(raw_node, dummy_node, lenv)

      when :constant_read_node, :constant_path_node
        ConstantReadNode.new(raw_node, lenv)

      when :self_node then SelfNode.new(raw_node, lenv)
      when :nil_node then NilNode.new(raw_node, lenv)
      when :true_node then TrueNode.new(raw_node, lenv)
      when :false_node then FalseNode.new(raw_node, lenv)
      when :integer_node then IntegerNode.new(raw_node, lenv)
      when :float_node then FloatNode.new(raw_node, lenv)
      when :rational_node then RationalNode.new(raw_node, lenv)
      when :imaginary_node then ComplexNode.new(raw_node, lenv)
      when :source_file_node then StringNode.new(raw_node, lenv, "")
      when :source_line_node then IntegerNode.new(raw_node, lenv, 0)
      when :source_encoding_node then SourceEncodingNode.new(raw_node, lenv)
      when :symbol_node then SymbolNode.new(raw_node, lenv)
      when :interpolated_symbol_node then InterpolatedSymbolNode.new(raw_node, lenv)
      when :string_node then StringNode.new(raw_node, lenv, raw_node.content)
      when :interpolated_string_node then InterpolatedStringNode.new(raw_node, lenv)
      when :x_string_node then StringNode.new(raw_node, lenv, "")
      when :interpolated_x_string_node then InterpolatedStringNode.new(raw_node, lenv)
      when :regular_expression_node then RegexpNode.new(raw_node, lenv)
      when :interpolated_regular_expression_node then InterpolatedRegexpNode.new(raw_node, lenv)

      when :array_node then ArrayNode.new(raw_node, lenv) # for %w[foo bar]
      when :range_node then RangeNode.new(raw_node, lenv) # TODO: support range pattern correctly

      else
        raise "unknown pattern node type: #{ raw_node.type }"
      end
    end

    def self.parse_cpath(raw_node, cref)
      names = []
      while raw_node
        case raw_node.type
        when :constant_read_node
          names << raw_node.name
          break
        when :constant_path_node, :constant_path_target_node
          if raw_node.parent
            names << raw_node.name
            raw_node = raw_node.parent
          else
            return names.reverse
          end
        when :self_node
          break if cref.scope_level == :class
          return nil
        else
          return nil
        end
      end
      cpath = cref.cpath # annotate
      return cpath + names.reverse if cpath
    end

    def self.parse_rbs(path, src)
      _buffer, _directives, raw_decls = RBS::Parser.parse_signature(src)

      cref = CRef::Toplevel
      lenv = LocalEnv.new(path, cref, {}, [])

      raw_decls.map do |raw_decl|
        AST.create_rbs_decl(raw_decl, lenv)
      end
    end

    def self.create_rbs_decl(raw_decl, lenv)
      case raw_decl
      when RBS::AST::Declarations::Class
        SigClassNode.new(raw_decl, lenv)
      when RBS::AST::Declarations::Module
        SigModuleNode.new(raw_decl, lenv)
      when RBS::AST::Declarations::Interface
        SigInterfaceNode.new(raw_decl, lenv)
      when RBS::AST::Declarations::Constant
        SigConstNode.new(raw_decl, lenv)
      when RBS::AST::Declarations::AliasDecl
      when RBS::AST::Declarations::TypeAlias
        SigTypeAliasNode.new(raw_decl, lenv)
        # TODO: check
      when RBS::AST::Declarations::Global
        SigGlobalVariableNode.new(raw_decl, lenv)
      else
        raise "unsupported: #{ raw_decl.class }"
      end
    end

    def self.create_rbs_member(raw_decl, lenv)
      case raw_decl
      when RBS::AST::Members::MethodDefinition
        SigDefNode.new(raw_decl, lenv)
      when RBS::AST::Members::Include
        SigIncludeNode.new(raw_decl, lenv)
      when RBS::AST::Members::Prepend
        SigPrependNode.new(raw_decl, lenv)
      when RBS::AST::Members::Extend
      when RBS::AST::Members::Public
      when RBS::AST::Members::Private
      when RBS::AST::Members::Alias
        SigAliasNode.new(raw_decl, lenv)
      when RBS::AST::Members::AttrReader
        SigAttrReaderNode.new(raw_decl, lenv)
      when RBS::AST::Members::AttrWriter
        SigAttrWriterNode.new(raw_decl, lenv)
      when RBS::AST::Members::AttrAccessor
        SigAttrAccessorNode.new(raw_decl, lenv)
      when RBS::AST::Declarations::Base
        self.create_rbs_decl(raw_decl, lenv)
      when RBS::AST::Members::InstanceVariable
        SigInstanceVariableNode.new(raw_decl, lenv, false)
      when RBS::AST::Members::ClassInstanceVariable
        SigInstanceVariableNode.new(raw_decl, lenv, true)
      when RBS::AST::Members::ClassVariable
        SigClassVariableNode.new(raw_decl, lenv)
      else
        raise "unsupported: #{ raw_decl.class }"
      end
    end

    def self.create_rbs_func_type(raw_decl, raw_type_params, raw_block, lenv)
      SigFuncType.new(raw_decl, raw_type_params, raw_block, lenv)
    end

    def self.create_rbs_type(raw_decl, lenv)
      case raw_decl
      when RBS::Types::Bases::Nil
        SigTyBaseNilNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Bool
        SigTyBaseBoolNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Self
        SigTyBaseSelfNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Void
        SigTyBaseVoidNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Any
        SigTyBaseAnyNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Top
        SigTyBaseTopNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Bottom
        SigTyBaseBottomNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Instance
        SigTyBaseInstanceNode.new(raw_decl, lenv)
      when RBS::Types::Bases::Class
        SigTyBaseClassNode.new(raw_decl, lenv)

      when RBS::Types::Alias
        SigTyAliasNode.new(raw_decl, lenv)
      when RBS::Types::Union
        SigTyUnionNode.new(raw_decl, lenv)
      when RBS::Types::Intersection
        SigTyIntersectionNode.new(raw_decl, lenv)
      when RBS::Types::ClassSingleton
        SigTySingletonNode.new(raw_decl, lenv)
      when RBS::Types::ClassInstance
        SigTyInstanceNode.new(raw_decl, lenv)
      when RBS::Types::Tuple
        SigTyTupleNode.new(raw_decl, lenv)
      when RBS::Types::Record
        SigTyRecordNode.new(raw_decl, lenv)
      when RBS::Types::Interface
        SigTyInterfaceNode.new(raw_decl, lenv)
      when RBS::Types::Proc
        SigTyProcNode.new(raw_decl, lenv)
      when RBS::Types::Variable
        SigTyVarNode.new(raw_decl, lenv)
      when RBS::Types::Optional
        SigTyOptionalNode.new(raw_decl, lenv)
      when RBS::Types::Literal
        SigTyLiteralNode.new(raw_decl, lenv)
      else
        raise "unknown RBS type: #{ raw_decl.class }"
      end
    end
  end
end
