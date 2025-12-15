module Funicular
  module VDOM
    class Differ
      def self.diff(old_node, new_node)
        return [[:replace, new_node]] if old_node.nil?
        return [[:remove]] if new_node.nil?

        if old_node.is_a?(VNode) && new_node.is_a?(VNode)
          if old_node.type != new_node.type
            return [[:replace, new_node]]
          end

          case old_node.type
          when :text
            # @type var old_node: Funicular::VDOM::Text
            # @type var new_node: Funicular::VDOM::Text
            diff_text(old_node, new_node)
          when :element
            # @type var old_node: Funicular::VDOM::Element
            # @type var new_node: Funicular::VDOM::Element
            diff_element(old_node, new_node)
          when :component
            # @type var old_node: Funicular::VDOM::Component
            # @type var new_node: Funicular::VDOM::Component
            diff_component(old_node, new_node)
          else
            [[:replace, new_node]]
          end
        else
          if old_node == new_node
            []
          else
            [[:replace, new_node]]
          end
        end
      end

      private

      def self.diff_text(old_node, new_node)
        if old_node.content != new_node.content
          [[:replace, new_node]]
        else
          []
        end
      end

      def self.diff_element(old_node, new_node)
        patches = []

        if old_node.tag != new_node.tag
          return [[:replace, new_node]]
        end

        props_patch = diff_props(old_node.props, new_node.props)
        patches << [:props, props_patch] unless props_patch.empty?

        child_patches = diff_children(old_node.children, new_node.children)
        patches += child_patches unless child_patches.empty?

        patches
      end

      def self.diff_props(old_props, new_props)
        patches = {}
        new_props.each do |key, value|
          patches[key] = value if old_props[key] != value
        end
        old_props.keys.each do |key|
          patches[key] = nil unless new_props.keys.include?(key)
        end
        patches
      end

      def self.diff_children(old_children, new_children)
        patches = []
        max_length = [old_children.length, new_children.length].max
        max_length&.times do |i|
          old_child = old_children[i]
          new_child = new_children[i]
          child_patches = diff(old_child, new_child)
          patches << [i, child_patches] unless child_patches.empty?
        end
        patches
      end

      def self.diff_component(old_node, new_node)
        if old_node.component_class != new_node.component_class
          return [[:replace, new_node]]
        end

        # If the component is marked for preservation,
        # update its internal VDOM instead of replacing the whole component.
        if new_node.props[:preserve]
          instance = old_node.instance
          return [[:replace, new_node]] unless instance # Fallback if instance is missing

          new_node.instance = instance # Preserve the instance

          # If props are unchanged, no need to diff internal VDOM
          return [] if old_node.props == new_node.props

          old_internal_vdom = instance.instance_variable_get(:@vdom)
          instance.instance_variable_set(:@props, new_node.props)
          new_internal_vdom = instance.send(:build_vdom)
          instance.instance_variable_set(:@vdom, new_internal_vdom)
          internal_patches = diff(old_internal_vdom, new_internal_vdom)

          # Return a special patch that instructs the patcher to re-bind events
          return [[:update_and_rebind, instance, internal_patches]]
        end

        # Original logic: If props changed, replace the entire component
        if old_node.props != new_node.props
          return [[:replace, new_node]]
        end

        []
      end
    end
  end
end
