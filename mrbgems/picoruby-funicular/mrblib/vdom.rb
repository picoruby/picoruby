module Funicular
  module VDOM
    BOOLEAN_ATTRIBUTES = %w[
      disabled checked selected readonly required autofocus multiple
    ]

    class VNode
      attr_reader :type, :key

      def initialize(type)
        @type = type
      end
    end

    class Element < VNode
      attr_reader :tag, :props, :children

      def initialize(tag, props = {}, children = [])
        super(:element)
        @tag = tag.to_s
        @key = props.delete(:key)
        @props = props || {}
        @children = normalize_children(children || [])
      end

      private

      def normalize_children(children)
        result = []
        children.each do |child|
          case child
          when VNode
            result << child
          when String
            result << child
          when Array
            # Flatten arrays (typically from .each or .map return values)
            # Recursively normalize nested arrays
            # @type var child: Array[Funicular::VDOM::child_t]
            result.concat(normalize_children(child))
          when nil
            # Skip nil values
          else
            # Convert other types to strings
            result << child.to_s
          end
        end
        result
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

    class Component < VNode
      attr_reader :component_class, :props
      attr_accessor :instance

      def initialize(component_class, props = {})
        super(:component)
        @component_class = component_class
        @key = props.delete(:key)
        @props = props
        @instance = nil
      end

      def ==(other)
        return false unless other.is_a?(Component)
        @component_class == other.component_class && @props == other.props
      end
    end

    class Renderer
      def initialize(doc = nil)
        @doc = doc || JS.document
        @error_boundary_stack = []
      end

      def render(vnode, parent = nil)
        case vnode&.type
        when :element
          # @type var vnode: Funicular::VDOM::Element
          render_element(vnode, parent)
        when :text
          # @type var vnode: Funicular::VDOM::Text
          render_text(vnode, parent)
        when :component
          # @type var vnode: Funicular::VDOM::Component
          render_component(vnode, parent)
        else
          raise "Unknown vnode type: #{vnode&.type}"
        end
      end

      # Find the nearest error boundary instance on the stack
      def current_error_boundary
        @error_boundary_stack.last
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
          elsif BOOLEAN_ATTRIBUTES.include?(key_str)
            # Handle boolean attributes
            if value.nil? || value.to_s == "false"
              # Do not set attribute (leave it absent)
            else
              dom_node.setAttribute(key_str, key_str)
            end
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

      def render_component(component_vnode, parent)
        instance = component_vnode.component_class.new(component_vnode.props)
        component_vnode.instance = instance

        is_error_boundary = instance.is_a?(Funicular::ErrorBoundary)

        # Push error boundary to stack if this component is one
        @error_boundary_stack.push(instance) if is_error_boundary

        begin
          component_vdom = instance.build_vdom
          dom_node = render(component_vdom, parent)

          # Check if this ErrorBoundary caught an error during child rendering
          # If so, its @vdom was already set to fallback in the rescue block
          error_was_caught = is_error_boundary && instance.error_caught_during_render

          if error_was_caught
            # ErrorBoundary caught an error - use the fallback vdom/dom that were set in rescue
            # Note: The div.error-boundary-content created during initial render
            # will be orphaned, but that's acceptable as it's not attached to the DOM
            fallback_vdom = instance.vdom
            fallback_dom = instance.dom_element

            # Bind events on the fallback DOM
            instance.bind_events(fallback_dom, fallback_vdom)
            instance.collect_refs(fallback_dom, fallback_vdom)

            # Return the fallback DOM
            fallback_dom
          else
            # Normal case - store VDOM and DOM element
            instance.vdom = component_vdom
            instance.dom_element = dom_node
            instance.bind_events(dom_node, component_vdom)
            instance.collect_refs(dom_node, component_vdom)
            dom_node
          end
        rescue => e
          # Pop error boundary from stack before handling
          @error_boundary_stack.pop if is_error_boundary

          # Try to find an error boundary to handle this error
          boundary = current_error_boundary
          if boundary && !is_error_boundary
            error_info = {
              component_class: component_vnode.component_class.to_s,
              props: component_vnode.props
            }

            # Let the error boundary handle the error
            boundary.catch_error(e, error_info)

            # Re-render the error boundary with fallback UI
            boundary_vdom = boundary.build_vdom
            fallback_dom = render(boundary_vdom, nil)

            # Update boundary's internal state
            boundary.vdom = boundary_vdom
            boundary.dom_element = fallback_dom
            boundary.mounted = true
            boundary.bind_events(fallback_dom, boundary_vdom)

            fallback_dom
          else
            # No error boundary to catch this error, let it propagate
            raise e
          end
        ensure
          # Pop error boundary from stack after successful render
          @error_boundary_stack.pop if is_error_boundary && @error_boundary_stack.last == instance
        end
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
