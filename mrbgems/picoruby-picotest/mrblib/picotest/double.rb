module Picotest

  class Double
    attr_accessor :double_data, :doubled_obj, :singleton_class_name

    def self._init(type, doubled_obj, any_instance_of: false)
      instance = _alloc(doubled_obj)
      instance.doubled_obj = doubled_obj
      instance.double_data = {
        type: type,
        any_instance_of: any_instance_of,
        doubled_obj_id: doubled_obj.object_id,
        method_id: nil,
        return_value: nil,
        singleton_class_name: nil
      }
      if type == :mock
        instance.double_data[:expected_count] = 0
        instance.double_data[:actual_count] = 0
      end
      unless $picotest_doubles
        $picotest_doubles = []
      end
      $picotest_doubles << instance.double_data
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
      if @double_data[:any_instance_of]
        @singleton_class_name = define_method_any_instance_of(method_id, @doubled_obj)
      else
        @singleton_class_name = define_method(method_id, @doubled_obj)
      end
      return self
    end

    self.instance_methods.each do |m|
      %i(method_missing
        instance_methods
        _init
        _alloc
        double_data
        doubled_obj
        singleton_class_name
        define_method
        define_method_any_instance_of
        remove_singleton
      ).include?(m) and next
      alias m :method_missing
    end

  end
end
