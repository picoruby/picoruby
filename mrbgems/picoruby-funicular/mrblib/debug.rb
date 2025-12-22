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

      def register_component(component)
        return nil unless enabled?
        @component_counter ||= 0
        id = (@component_counter += 1)
        component_registry[id] = component
        id
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
              %Q({"id":#{id},"class":"#{class_name}","state_keys":#{state_keys.inspect},"mounted":#{mounted},"children":#{child_ids.inspect}})
            rescue => e
              %Q({"id":#{id},"error":"#{e.message}"})
            end
          end
          "[#{components.join(',')}]"
        rescue => e
          "{\"error\":\"#{e.message}\"}"
        end
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
            collect_direct_children(child, children)
          end
        end
      end
    end
  end
end
