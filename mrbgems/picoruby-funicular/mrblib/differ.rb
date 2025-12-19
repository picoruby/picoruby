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
        # Check if any child has a key
        has_keys = new_children.any? { |child|
          child.is_a?(VNode) && child.respond_to?(:key) && child.key
        }

        if has_keys
          diff_children_with_keys(old_children, new_children)
        else
          diff_children_by_index(old_children, new_children)
        end
      end

      def self.diff_children_with_keys(old_children, new_children)
        patches = []

        # 1. Build key map from old children
        old_key_map = {}
        old_children.each_with_index do |child, index|
          if child.is_a?(VNode) && child.respond_to?(:key) && child.key
            old_key_map[child.key] = [index, child]
          end
        end

        # 2. Track matched old indices
        matched_old_indices = {}

        # 3. Process new children
        new_children.each_with_index do |new_child, new_index|
          if new_child.is_a?(VNode) && new_child.respond_to?(:key) && new_child.key
            key = new_child.key

            if old_key_map[key]
              old_index, old_child = old_key_map[key]
              matched_old_indices[old_index] = true

              # Diff existing element
              child_patches = diff(old_child, new_child)
              unless child_patches.empty?
                patches << [new_index, child_patches]
              end
            else
              # Insert new element
              patches << [new_index, [[:replace, new_child]]]
            end
          else
            # Fallback to index-based for elements without keys
            old_child = old_children[new_index]
            child_patches = diff(old_child, new_child)
            unless child_patches.empty?
              patches << [new_index, child_patches]
            end
          end
        end

        # 4. Remove unmatched old elements
        old_children.each_with_index do |old_child, old_index|
          next unless old_child.is_a?(VNode)
          next if matched_old_indices[old_index]

          if old_child.respond_to?(:key) && old_child.key
            patches << [old_index, [[:remove]]]
          end
        end

        # Sort patches: updates first, removes last (descending index)
        patches.sort { |a, b|
          a_is_remove = a[1].length == 1 && a[1][0] == [:remove]
          b_is_remove = b[1].length == 1 && b[1][0] == [:remove]

          if a_is_remove && b_is_remove
            b[0] <=> a[0]
          elsif a_is_remove
            1
          elsif b_is_remove
            -1
          else
            a[0] <=> b[0]
          end
        }
      end

      def self.diff_children_by_index(old_children, new_children)
        patches = []
        remove_patches = []
        max_length = [old_children.length, new_children.length].max
        max_length&.times do |i|
          old_child = old_children[i]
          new_child = new_children[i]
          child_patches = diff(old_child, new_child)
          unless child_patches.empty?
            # Check if this is a simple remove patch
            if child_patches.length == 1 && child_patches[0] == [:remove]
              remove_patches << [i, child_patches]
            else
              patches << [i, child_patches]
            end
          end
        end
        # Add remove patches at the end, sorted by index in descending order
        patches + remove_patches.sort { |a, b| b[0] <=> a[0] }
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
