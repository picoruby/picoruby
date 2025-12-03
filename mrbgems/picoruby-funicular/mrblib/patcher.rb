module Funicular
  module VDOM
    class Patcher
      def initialize(doc = nil)
        @doc = doc || JS.document
      end

      def apply(element, patches)
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
            if child_element.nil?
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
        result
      end

      private

      def update_props(element, props_patch)
        props_patch.each do |key, value|
          if value.nil?
            element.removeAttribute(key.to_s)
          else
            element.setAttribute(key.to_s, value.to_s)
          end
        end
      end

      def create_element(vnode)
        if vnode.is_a?(String)
          return @doc.createTextNode(vnode)
        end

        unless vnode.is_a?(VNode)
          raise "Invalid vnode: #{vnode.class}"
        end

        case vnode.type
        when :text
          if vnode.is_a?(Text)
            @doc.createTextNode(vnode.content)
          else
            raise "Expected Text vnode"
          end
        when :element
          if vnode.is_a?(Element)
            element = @doc.createElement(vnode.tag)

            vnode.props.each do |key, value|
              element.setAttribute(key.to_s, value.to_s)
            end

            vnode.children.each do |child|
              if child.is_a?(VNode)
                element.appendChild(create_element(child))
              elsif child.is_a?(String)
                text_node = @doc.createTextNode(child)
                element.appendChild(text_node)
              elsif child.is_a?(Array)
                child.each do |c|
                  if c.is_a?(VNode)
                    element.appendChild(create_element(c))
                  elsif c.is_a?(String)
                    text_node = @doc.createTextNode(c)
                    element.appendChild(text_node)
                  end
                end
              end
            end

            element
          else
            raise "Expected Element vnode"
          end
        else
          raise "Unknown vnode type: #{vnode.type}"
        end
      end
    end
  end
end
