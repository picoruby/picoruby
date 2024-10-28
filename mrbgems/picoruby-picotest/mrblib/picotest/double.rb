module Picotest

  class Double
    attr_accessor :double_data, :doubled_obj, :singleton_class_name

    def self.alloc(type, doubled_obj)
      instance = _alloc(doubled_obj)
      instance.doubled_obj = doubled_obj
      instance.double_data = {
        type: type,
        doubled_obj_id: doubled_obj.object_id,
        method_id: nil,
        return_value: nil,
        singleton_class_name: nil
      }
      if type == :mock
        instance.double_data[:expected_count] = 0
        instance.double_data[:actual_count] = 0
      end
      double_methods = get_double_methods
      double_methods << instance.double_data
      instance
    end

    def method_missing(method_id = Kernel.caller(0, 1)[0].to_sym, expected_count = 1,&block)
                        #                        ^
                        # `method_mising' is not in callinfo link
      @method_id = method_id
      @double_data[:method_id] = method_id
      @double_data[:return_value] = block.call if block
      if @double_data[:type] == :mock
        @double_data[:expected_count] = expected_count
      end
      @singleton_class_name = _define_method(method_id, @doubled_obj)
      return self
    end

    alias ! method_missing
    alias != method_missing
    alias <=> method_missing
    alias == method_missing
    alias === method_missing
    alias __id__ method_missing
    alias ` method_missing
    alias ancestors method_missing
    alias attr_accessor method_missing
    alias attr_reader method_missing
    alias block_given? method_missing
    alias caller method_missing
    alias class method_missing
    alias class? method_missing
    alias const_get method_missing
    alias dup method_missing
    alias extend method_missing
    alias extern method_missing
    alias gets method_missing
    alias include method_missing
    alias inspect method_missing
    alias instance_methods method_missing
    alias instance_of? method_missing
    alias instance_variable_get method_missing
    alias instance_variable_set method_missing
    alias instance_variables method_missing
    alias instance_variables method_missing
    alias is_a? method_missing
    alias kind_of? method_missing
    alias load method_missing
    alias load_file method_missing
    alias loop method_missing
    alias methods method_missing
    alias new method_missing
    alias nil? method_missing
    alias object_id method_missing
    alias open method_missing
    alias p method_missing
    alias print method_missing
    alias printf method_missing
    alias puts method_missing
    alias raise method_missing
    alias require method_missing
    alias require_file method_missing
    alias required? method_missing
    alias respond_to? method_missing
    alias send method_missing
    alias sleep method_missing
    alias sleep_ms method_missing
    alias sprintf method_missing
    alias to_s method_missing
  end

end
