module Rubino
  class Patcher
    def self.apply(element, patches)
      return element if patches.empty?
      result = element
      patches.each do |patch|
        case patch[0]
        when :replace
          new_element = create_element(patch[1])
          if parent = element.parentElement
            parent.replaceChild(new_element, element)
            result = new_element
          end
        when :props
          update_props(element, patch[1])
        when :remove
          element.parentElement.removeChild(element)
        when Integer
          child_index = patch[0]
          child_patches = patch[1]
          children = element.children.to_poro
          child_element = children.is_a?(Array) ? children[child_index] : nil
          if child_element.nil? # No children tag, but possibly text
            case child_patches[0][0]
            when :replace
              new_child_element = create_element(child_patches[0][1])
              element.innerHTML = ""
              element.appendChild(new_child_element)
            when :remove
              element.innerHTML = ""
            when Integer
              new_child_element = create_element(child_patches[0][1])
              element.appendChild(new_child_element)
            end
          else
            apply(child_element, child_patches)
          end
        end
      end
      result # Return value is used only in the top level
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
        return JS.document.createTextNode(vnode.children.map{|c|c.to_s}.join)
      end

      element = JS.document.createElement(vnode.type)

      vnode.props.each do |key, value|
        element.setAttribute(key.to_s, value.to_s)
      end

      vnode.children.each do |child|
        if child.is_a?(VNode)
          element.appendChild(create_element(child))
        else
          puts "TODO: handle text nodes"
        end
      end

      element
    end
  end
end
