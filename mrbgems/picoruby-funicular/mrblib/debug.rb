module Funicular
  module Debug
    class << self
      attr_accessor :enabled

      def enabled?
        @enabled ||= Funicular.env.development?
      end

      def component_registry
        @component_registry ||= {}
      end

      def error_registry
        @error_registry ||= []
      end

      def register_component(component)
        return nil unless enabled?
        @component_counter ||= 0
        id = (@component_counter += 1)
        component_registry[id] = component
        id
      end

      # Report an error caught by an ErrorBoundary
      def report_error(boundary, error, error_info = nil)
        return unless enabled?

        error_entry = {
          id: (error_registry.length + 1),
          timestamp: Time.now.to_s,
          boundary_id: boundary.instance_variable_get(:@__debug_id__),
          boundary_class: boundary.class.to_s,
          error_class: error.class.to_s,
          error_message: error.message,
          component_class: error_info&.dig(:component_class),
          backtrace: error.backtrace&.first(10)
        }

        error_registry << error_entry

        # Log to console
        puts "[ErrorBoundary] Caught error in #{error_info&.dig(:component_class) || 'unknown'}: #{error.message}"

        # Keep only last 50 errors
        @error_registry = error_registry.last(50) if error_registry.length > 50

        error_entry
      end

      # Clear all recorded errors
      def clear_errors
        @error_registry = []
      end

      # Get all recorded errors as JSON
      def error_list
        return "[]" unless enabled?

        begin
          errors = error_registry.map do |entry|
            pairs = []
            entry.each do |key, value|
              if value.is_a?(Array)
                escaped_arr = value.map { |v| %Q("#{v.to_s.gsub('"', '\\"')}") }.join(',')
                pairs << %Q("#{key}":[#{escaped_arr}])
              else
                escaped_value = value.to_s.gsub('\\', '\\\\\\\\').gsub('"', '\\"')
                pairs << %Q("#{key}":"#{escaped_value}")
              end
            end
            "{#{pairs.join(',')}}"
          end
          "[#{errors.join(',')}]"
        rescue => e
          %Q([{"error":"#{e.message}"}])
        end
      end

      # Get the most recent error
      def last_error
        return nil unless enabled?
        error_registry.last
      end

      def unregister_component(id)
        return unless enabled?
        component_registry.delete(id)
      end

      def get_component(id)
        return nil unless enabled?
        component_registry[id]
      end

      def all_components
        return [] unless enabled?
        component_registry.values
      end

      def component_tree
        return "[]" unless enabled?

        begin
          components = component_registry.map do |id, component|
            begin
              mounted = component.instance_variable_get(:@mounted)
              state_keys = get_state_keys(component)
              class_name = component.class.to_s
              child_ids = get_child_ids(component)
              child_ids_json = "[#{child_ids.join(',')}]"

              # Check if this is an ErrorBoundary and if it has caught an error
              is_error_boundary = component.is_a?(Funicular::ErrorBoundary)
              has_error = is_error_boundary && component.instance_variable_get(:@state)&.dig(:has_error)

              base = %Q("id":#{id},"class":"#{class_name}","state_keys":#{state_keys.inspect},"mounted":#{mounted},"children":#{child_ids_json})
              if is_error_boundary
                base += %Q(,"is_error_boundary":true,"has_error":#{has_error})
              end
              "{#{base}}"
            rescue => e
              %Q({"id":#{id},"error":"#{e.message}"})
            end
          end
          "[#{components.join(',')}]"
        rescue => e
          "{\"error\":\"#{e.message}\"}"
        end
      end

      # Get error count
      def error_count
        return 0 unless enabled?
        error_registry.length
      end

      def get_component_state(id)
        return "{}" unless enabled?
        component = get_component(id)
        return "{}" unless component

        state = component.instance_variable_get(:@state) || {}
        pairs = []

        state.each do |key, value|
          begin
            escaped_value = value.inspect.gsub('\\', '\\\\\\\\').gsub('"', '\\"')
            pairs << %Q("#{key}":"#{escaped_value}")
          rescue
            pairs << %Q("#{key}":"<error inspecting value>")
          end
        end

        "{#{pairs.join(',')}}"
      end

      def get_component_instance_variables(id)
        return "{}" unless enabled?
        component = get_component(id)
        return "{}" unless component

        pairs = []
        component.instance_variables.each do |var|
          next if var == :@state # State is handled separately
          next if var.to_s.start_with?('@__debug') # Skip debug-internal variables
          if var == :@vdom || var == :@child_components
            pairs << %Q("#{var}":"<omitted>")
            next
          end

          begin
            value = component.instance_variable_get(var)
            escaped_value = value.inspect.gsub('\\', '\\\\\\\\').gsub('"', '\\"')
            pairs << %Q("#{var}":"#{escaped_value}")
          rescue
            pairs << %Q("#{var}":"<error inspecting value>")
          end
        end

        "{#{pairs.join(',')}}"
      end

      def expose_to_global
        return unless enabled?
        # Export to global variable for DevTools access
        $__funicular_debug__ = self
      end

      private

      def get_state_keys(component)
        state = component.instance_variable_get(:@state)
        return [] unless state.is_a?(Hash)
        state.keys.map(&:to_s)
      end

      def get_child_ids(component)
        # Get only direct children by scanning component's vdom
        vdom = component.instance_variable_get(:@vdom)
        return [] unless vdom

        direct_children = []
        collect_direct_children(vdom, direct_children)
        direct_children.map { |child| child.instance_variable_get(:@__debug_id__) }.compact
      end

      def collect_direct_children(vnode, children)
        if vnode.is_a?(VDOM::Component)
          # Found a direct child component, don't recurse further
          children << vnode.instance if vnode.instance
        elsif vnode.is_a?(VDOM::Element)
          # Keep looking through elements
          vnode.children&.each do |child|
            # @type var child: VDOM::VNode
            collect_direct_children(child, children)
          end
        end
      end
    end
  end
end
