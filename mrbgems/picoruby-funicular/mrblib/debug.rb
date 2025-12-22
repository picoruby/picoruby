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
        return [] unless enabled?
        component_registry.map do |id, component|
          {
            id: id,
            class: component.class.name,
            state_keys: get_state_keys(component),
            mounted: component.instance_variable_get(:@mounted)
          }
        end
      end

      def get_component_state(id)
        return {} unless enabled?
        component = get_component(id)
        return {} unless component

        state = component.instance_variable_get(:@state) || {}
        result = {}

        state.each do |key, value|
          begin
            result[key.to_s] = value.inspect
          rescue
            result[key.to_s] = "<error inspecting value>"
          end
        end

        result
      end

      def get_component_instance_variables(id)
        return {} unless enabled?
        component = get_component(id)
        return {} unless component

        result = {}
        component.instance_variables.each do |var|
          next if var == :@state # State is handled separately
          next if var.to_s.start_with?('@__debug') # Skip debug-internal variables

          begin
            value = component.instance_variable_get(var)
            result[var.to_s] = value.inspect
          rescue
            result[var.to_s] = "<error inspecting value>"
          end
        end

        result
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
    end
  end
end
