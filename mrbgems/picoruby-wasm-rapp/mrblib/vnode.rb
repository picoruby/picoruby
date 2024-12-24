module Rapp
  class VNode
    attr_reader :type, :props, :children

    def initialize(type, props = {}, children = [])
      @type = type
      @props = props
      if type == '#text'
        @children = children
      else
        @children = []
        children.each do |child|
          case child
          when VNode
            @children << child
          else
            @children << VNode.create(child)
          end
        end
      end
    end

    def self.create(node)
      case node
      when VNode
        node
      when String, Integer, Float
        VNode.new('#text', {}, node.to_s)
      when Array
        node.map { |n| VNode.create(n) }
      else
        raise "Unsupported node type: #{node.class}"
      end
    end

    def to_s
      "#{@type} #{@props} #{@children.is_a?(String) ? @children : @children.map{|c| c.to_s}}"
    end
  end

  class Differ
    def self.diff(old_node, new_node)
      return [[:replace, new_node]] if old_node.nil?
      return [[:remove]] if new_node.nil?
      return [] if old_node == new_node

      if old_node.is_a?(String) || new_node.is_a?(String)
        return old_node.to_s == new_node.to_s ? [] : [[:replace, new_node]]
      end

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

  class Patcher
    def self.apply(element, patches)
      #return unless element
      return element if patches.empty?
      result = element
      patches.each do |patch|
        case patch[0]
        when :replace
          new_element = create_element(patch[1])
          if element.parent
            element.parent.replaceChild(new_element, element)
            result = new_element
          end
        when :props
          update_props(element, patch[1])
        when :remove
          element.parent&.removeChild(element)
          result = nil
        else # Integer: child index
          child_index = patch[0]
          child_patches = patch[1]
          case child_patches[0][0]
          when Integer
            child_element = element.children.to_poro[child_index]
            apply(child_element, child_patches)
          when :replace
            if child_patches[0][1].type == '#text'
              element.textContent = child_patches[0][1].children.to_s
            else
              child_element = element.children.to_poro[child_index]
              if child_element
                apply(child_element, child_patches)
              else
                new_child = create_element(child_patches[0][1])
                element.appendChild(new_child)
              end
            end
          when :props
            update_props(element.children.to_poro[child_index], child_patches[0][1])
          when :remove
            element.children.to_poro[child_index].remove
          else
            raise "Unsupported patch type: #{child_patches[0][0]}"
          end
        end
      end
      result
    end

    # private

    def self.update_props(element, props_patch)
      props_patch.each do |key, value|
        if value.nil?
          element.removeAttribute(key.to_s)
        else
          element.setAttribute(key.to_s, value.to_s)
        end
      end
    end

    def self.create_element(vnode)
      if vnode.type == '#text'
        return JS.global[:document].createTextNode(vnode.children.to_s)
      end

      element = JS.global[:document].createElement(vnode.type)
      vnode.props.each do |key, value|
        element.setAttribute(key.to_s, value.to_s)
      end
      vnode.children.each do |child|
        child_node = VNode.create(child)
        element.appendChild(create_element(child_node))
      end
      element
    end

  end
end
