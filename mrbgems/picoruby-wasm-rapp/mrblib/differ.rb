module Rapp
  class Differ
    def self.diff(old_node, new_node)
      return [[:replace, new_node]] if old_node.nil?
      return [[:remove]] if new_node.nil?
      return [] if old_node == new_node

      patches = []
      if old_node.type != new_node.type
        patches << [:replace, new_node]
      else
        props_patch = diff_props(old_node.props, new_node.props)
        patches << [:props, props_patch] unless props_patch.empty?

        if old_node.type == '#text'
          if old_node.children != new_node.children
            patches << [:replace, new_node]
          end
        else
          child_patches = diff_children(old_node.children, new_node.children)
          patches += child_patches unless child_patches.empty?
        end
      end

      patches
    end
 
    # private

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
      unless old_children.is_a?(Array) && new_children.is_a?(Array)
        puts "WARN"
        return patches
      end
      max_length = [old_children.length, new_children.length].max
      max_length&.times do |i|
        old_child = old_children[i]
        new_child = new_children[i]
        child_patches = diff(old_child, new_child)
        patches << [i, child_patches] unless child_patches.empty?
      end
      patches
    end
  end
end
