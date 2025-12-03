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
            diff_text(old_node, new_node)
          when :element
            diff_element(old_node, new_node)
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
        max_length.times do |i|
          old_child = old_children[i]
          new_child = new_children[i]
          child_patches = diff(old_child, new_child)
          patches << [i, child_patches] unless child_patches.empty?
        end
        patches
      end
    end
  end
end
