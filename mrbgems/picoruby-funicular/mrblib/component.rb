module Funicular
  class Component
    attr_reader :state, :props, :refs

    def initialize(props = {})
      @props = props
      @state = initialize_state || {}
      @vdom = nil
      @dom_element = nil
      @refs = {}
      @event_listeners = []
      @mounted = false
      @updating = false
    end

    # Override this method in subclasses to define initial state
    def initialize_state
      {}
    end

    # Update state and trigger re-render
    def update(new_state)
      return unless @mounted
      return if @updating

      begin
        @updating = true
        component_will_update if respond_to?(:component_will_update)

        @state = @state.merge(new_state)
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

    # Re-render component (called by update)
    def re_render
      return unless @mounted

      new_vdom = build_vdom
      patches = VDOM::Differ.diff(@vdom, new_vdom)

      unless patches.empty?
        cleanup_events
        @dom_element = VDOM::Patcher.new.apply(@dom_element, patches)
        bind_events(@dom_element, new_vdom)
        collect_refs(@dom_element, new_vdom)
      end

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
        children = value.map { |v| normalize_vnode(v) }.compact
        VDOM::Element.new("div", {}, children)
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
              value.call(event)
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
        JS.global.removeEventListener(callback_id)
      end
      @event_listeners = []
    end

    # DSL methods for HTML elements
    HTML_TAGS = %w[
      div span p a
      h1 h2 h3 h4 h5 h6
      ul ol li
      table thead tbody tr th td
      form input textarea button select option
      header footer nav section article aside
      img video audio
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
        if @rendering && prev_children
          prev_children << element
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
