module Funicular
  class Component
    class StateAccessor
      def initialize(state_hash)
        @state = state_hash
      end

      # Support dynamic key access: state[:key] or state[variable_key]
      def [](key)
        @state[key]
      end

      def method_missing(method, *args)
        if method.to_s.end_with?('=')
          key = method.to_s[0..-2] || method
          raise "Use patch(#{key}: value) to update state"
        else
          @state[method]
        end
      end

      def respond_to_missing?(method, include_private = false)
        return false if method.to_s.end_with?('=')
        @state.key?(method) || super
      end
    end

    attr_reader :props, :refs

    def initialize(props = {})
      @props = props
      @state = initialize_state || {}
      @state_accessor = nil
      @vdom = nil
      @dom_element = nil
      @refs = {}
      @event_listeners = []
      @mounted = false
      @updating = false
    end

    def state
      @state_accessor ||= StateAccessor.new(@state)
    end

    # Override this method in subclasses to define initial state
    def initialize_state
      {}
    end

    # Update state and trigger re-render
    def patch(new_state)
      return unless @mounted
      return if @updating

      begin
        @updating = true
        component_will_update if respond_to?(:component_will_update)

        # Convert JS::Object values to Ruby native types automatically
        normalized_state = {}
        new_state.each do |key, value|
          normalized_state[key] = normalize_state_value(value)
        end

        @state = @state.merge(normalized_state)
        @state_accessor = nil  # Invalidate accessor to reflect new state
        re_render

        component_updated if respond_to?(:component_updated)
      rescue => e
        component_raised(e) if respond_to?(:component_raised)
        raise e
      ensure
        @updating = false
      end
    end

    # Mount component to a DOM container
    def mount(container)
      return if @mounted

      begin
        component_will_mount if respond_to?(:component_will_mount)

        @container = container
        new_vdom = build_vdom
        @dom_element = VDOM::Renderer.new.render(new_vdom)
        bind_events(@dom_element, new_vdom)
        collect_refs(@dom_element, new_vdom)
        container.appendChild(@dom_element)
        @vdom = new_vdom
        @mounted = true

        component_mounted if respond_to?(:component_mounted)
      rescue => e
        component_raised(e) if respond_to?(:component_raised)
        raise e
      end
    end

    # Unmount component from DOM
    def unmount
      return unless @mounted

      begin
        component_will_unmount if respond_to?(:component_will_unmount)

        cleanup_events
        @container.removeChild(@dom_element) if @container && @dom_element
        @mounted = false
        @dom_element = nil
        @vdom = nil
        @refs = {}

        component_unmounted if respond_to?(:component_unmounted)
      rescue => e
        component_raised(e) if respond_to?(:component_raised)
        raise e
      end
    end

    # Override this method in subclasses to define render logic
    def render
      raise "Subclasses must implement the render method"
    end

    private

    # Normalize state value by converting JS::Object to Ruby native types
    def normalize_state_value(value)
      # Check if value has to_poro method (JS::Object)
      if value.respond_to?(:to_poro)
        begin
          value.to_poro
        rescue
          # If to_poro fails, return the original value
          value
        end
      elsif value.is_a?(Hash)
        # Recursively normalize hash values
        normalized = {}
        value.each do |k, v|
          normalized[k] = normalize_state_value(v)
        end
        normalized
      elsif value.is_a?(Array)
        # Recursively normalize array elements
        value.map { |v| normalize_state_value(v) }
      else
        # Return as-is for Ruby native types
        value
      end
    end

    # Re-render component (called by update)
    def re_render
      return unless @mounted

      new_vdom = build_vdom
      patches = VDOM::Differ.diff(@vdom, new_vdom)

      # Always cleanup and rebind events to avoid stale event listeners
      cleanup_events

      unless patches.empty?
        @dom_element = VDOM::Patcher.new.apply(@dom_element, patches)
      end

      bind_events(@dom_element, new_vdom)
      collect_refs(@dom_element, new_vdom)

      @vdom = new_vdom
    end

    # Build VDOM tree from render method
    def build_vdom
      @rendering = true
      @current_children = nil
      result = render
      @rendering = false

      # Convert render result to VNode
      normalize_vnode(result)
    end

    # Normalize render result to VNode
    def normalize_vnode(value)
      case value
      when VDOM::VNode
        value
      when String
        VDOM::Text.new(value)
      when Integer, Float
        VDOM::Text.new(value.to_s)
      when Array
        # Arrays are typically return values from iterators like .each or .map
        # The elements have already been added to @current_children during iteration
        # Return nil to avoid duplicate rendering
        nil
      when nil
        puts "[WARN] Render returned nil, rendering empty text node"
        VDOM::Text.new("")
      else
        puts "[WARN] Unknown render value type: #{value.class}, converting to string"
        VDOM::Text.new(value.to_s)
      end
    end

    # Bind event handlers to DOM elements
    def bind_events(dom_element, vnode)
      return unless vnode.is_a?(VDOM::Element)

      vnode.props.each do |key, value|
        key_str = key.to_s
        next unless key_str.start_with?('on')

        event_name = key_str[2..-1]&.downcase || ""

        # addEventListener expects a block, not a Proc
        callback_id = case value
        when Symbol
          dom_element.addEventListener(event_name) do |event|
            begin
              self.send(value, event)
            rescue => e
              component_raised(e) if respond_to?(:component_raised)
              raise e
            end
          end
        when Proc
          dom_element.addEventListener(event_name) do |event|
            begin
              # @type var value: Proc
              # Check if Proc expects arguments (arity)
              # arity == 0: no arguments expected, call without event
              # arity >= 1 or arity < 0: arguments expected, call with event
              if value.arity == 0
                value.call()
              else
                value.call(event)
              end
            rescue => e
              component_raised(e) if respond_to?(:component_raised)
              raise e
            end
          end
        else
          raise "Invalid event handler: #{value.class}"
        end

        @event_listeners << callback_id
      end

      # Recursively bind events for children
      if vnode.children && dom_element.children
        children = dom_element.children.to_poro
        vnode.children.each_with_index do |child_vnode, index|
          if child_vnode.is_a?(VDOM::Element) && children.is_a?(Array)
            child_element = children[index]
            bind_events(child_element, child_vnode) if child_element
          end
        end
      end
    end

    # Collect ref elements from VDOM
    def collect_refs(dom_element, vnode, refs_map = {})
      return refs_map unless vnode.is_a?(VDOM::Element)

      if vnode.props[:ref]
        ref_name = vnode.props[:ref].to_sym
        @refs[ref_name] = dom_element
        refs_map[ref_name] = dom_element
      end

      if vnode.children && dom_element.children
        children = dom_element.children.to_poro
        vnode.children.each_with_index do |child_vnode, index|
          if child_vnode.is_a?(VDOM::Element) && children.is_a?(Array)
            child_element = children[index]
            collect_refs(child_element, child_vnode, refs_map) if child_element
          end
        end
      end

      refs_map
    end

    # Cleanup event listeners
    def cleanup_events
      @event_listeners.each do |callback_id|
        JS::Object.removeEventListener(callback_id)
      end
      @event_listeners = []
    end

    # DSL methods for HTML elements
    HTML_TAGS = %w[
      div span p a
      h1 h2 h3 h4 h5 h6
      ul ol li
      table thead tbody tr th td
      form input textarea button select option label
      header footer nav section article aside
      img video audio canvas
      br hr
    ]

    HTML_TAGS.each do |tag|
      define_method(tag) do |props = {}, &block|
        children = []

        if block
          prev_children = @current_children
          @current_children = children
          # @type var block: Proc
          result = block.call
          @current_children = prev_children

          # Add block result to children if not already added via add_child
          if result && !result.equal?(children) && children.empty?
            normalized = normalize_vnode(result)
            children << normalized if normalized
          end
        end

        # @type var props: Hash[Symbol, String]
        element = VDOM::Element.new(tag, props, children)

        # If we're inside another element's block, add this element to parent's children
        if @rendering && @current_children
          @current_children << element
        end

        element
      end
    end

    # Helper to add children in DSL blocks
    def add_child(child)
      return unless @rendering && @current_children

      normalized = normalize_vnode(child)
      @current_children << normalized if normalized
    end
  end
end
