module Funicular
  module VDOM
    class VNode
      attr_reader :type

      def initialize(type)
        @type = type
      end
    end

    class Element < VNode
      attr_reader :tag, :props, :children

      def initialize(tag, props = {}, children = [])
        super(:element)
        @tag = tag.to_s
        @props = props || {}
        @children = children || []
      end

      def ==(other)
        return false unless other.is_a?(Element)
        @tag == other.tag && @props == other.props && @children == other.children
      end
    end

    class Text < VNode
      attr_reader :content

      def initialize(content)
        super(:text)
        @content = content.to_s
      end

      def ==(other)
        return false unless other.is_a?(Text)
        @content == other.content
      end
    end

    class Renderer
      def initialize(doc = nil)
        @doc = doc || JS.document
      end

      def render(vnode, parent = nil)
        case vnode.type
        when :element
          render_element(vnode, parent)
        when :text
          render_text(vnode, parent)
        else
          raise "Unknown vnode type: #{vnode.type}"
        end
      end

      private

      def render_element(element, parent)
        dom_node = @doc.createElement(element.tag)

        element.props.each do |key, value|
          key_str = key.to_s
          if key_str.start_with?('on')
            # Event handlers are handled by Funicular::Component and should not be set as attributes.
            # warn "Funicular: Attempted to set event handler '#{key_str}' as an attribute. This will be ignored."
          elsif (key_str == 'href' || key_str == 'src') && value.to_s.start_with?('javascript:')
            # Prevent XSS attacks by blocking javascript: URIs in href/src attributes
            puts "[WARN] Funicular: Blocked potentially malicious value '#{value}' for attribute '#{key_str}'."
          else
            # Attribute
            dom_node.setAttribute(key_str, value.to_s)
          end
        end

        element.children.each do |child|
          if child.is_a?(VNode)
            child_dom = render(child)
            dom_node.appendChild(child_dom)
          elsif child.is_a?(String)
            text_node = @doc.createTextNode(child)
            dom_node.appendChild(text_node)
          elsif child.is_a?(Array)
            child.each do |c|
              if c.is_a?(VNode)
                child_dom = render(c)
                dom_node.appendChild(child_dom)
              elsif c.is_a?(String)
                text_node = @doc.createTextNode(c)
                dom_node.appendChild(text_node)
              end
            end
          end
        end

        parent.appendChild(dom_node) if parent

        dom_node
      end

      def render_text(text, parent)
        dom_node = @doc.createTextNode(text.content)
        parent.appendChild(dom_node) if parent
        dom_node
      end
    end

    def self.create_element(tag, props = {}, *children)
      Element.new(tag, props, children.flatten)
    end

    def self.create_text(content)
      Text.new(content)
    end

    def self.render(vnode, container)
      renderer = Renderer.new
      container.innerHTML = ''
      renderer.render(vnode, container)
    end

    def self.diff(old_vnode, new_vnode)
      Differ.diff(old_vnode, new_vnode)
    end

    def self.patch(element, patches)
      patcher = Patcher.new
      patcher.apply(element, patches)
    end
  end
end
