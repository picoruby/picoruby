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

    attr_accessor :props, :vdom, :dom_element, :mounted
    attr_reader :refs

    def initialize(props = {})
      @props = props
      @state = initialize_state || {}
      @state_accessor = nil
      @style_accessor = nil
      @vdom = nil
      @dom_element = nil
      @refs = {}
      @event_listeners = []
      @mounted = false
      @updating = false
      @child_components = []

      # Register component for debugging in development mode
      @__debug_id__ = Funicular::Debug.register_component(self) if Funicular::Debug.enabled?
    end

    def state
      @state_accessor ||= StateAccessor.new(@state)
    end

    # Override this method in subclasses to define initial state
    def initialize_state
      {}
    end

    # Class methods for styles DSL
    def self.styles(&block)
      builder = StyleBuilder.new
      builder.instance_eval(&block)
      @styles_definitions = builder.to_definitions
    end

    def self.styles_definitions
      @styles_definitions ||= {}
    end

    # Instance method to access styles
    def s
      @style_accessor ||= StyleAccessor.new(self.class.styles_definitions)
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
        collect_child_components(new_vdom)
        container.appendChild(@dom_element)
        @vdom = new_vdom
        @mounted = true

        # Mark child components as mounted and call their lifecycle hooks
        @child_components.each do |child|
          child.mounted = true
          child.component_mounted if child.respond_to?(:component_mounted)
        end

        component_mounted if respond_to?(:component_mounted)
      rescue => e
        component_raised(e) if respond_to?(:component_raised)
        raise e
      end
    end

    # Unmount component from DOM
    def unmount
      return unless @mounted
      # puts "==> Unmounting: #{self.class} id: #{@__debug_id__}"

      begin
        component_will_unmount if respond_to?(:component_will_unmount)

        # Unmount child components first
        # puts "  > Unmounting children: #{@child_components.map(&:class).join(', ')}"
        @child_components.each do |child|
          child.unmount if child.respond_to?(:unmount)
        end
        @child_components = []

        cleanup_events
        @container.removeChild(@dom_element) if @container && @dom_element
        @mounted = false
        @dom_element = nil
        @vdom = nil
        @refs = {}

        # Unregister component from debugging in development mode
        Funicular::Debug.unregister_component(@__debug_id__) if @__debug_id__

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

    # Build VDOM tree from render method
    # Called by VDOM::Renderer, Differ, and Patcher
    def build_vdom
      @rendering = true
      @current_children = nil
      result = render
      @rendering = false

      # Convert render result to VNode
      vnode = normalize_vnode(result)

      # Add data-component attribute to the root element
      if Funicular.env.development? && vnode
        add_data_component_attribute(vnode)
      end

      vnode
    end

    # Bind event handlers to DOM elements
    # Called by VDOM::Renderer and Patcher
    def bind_events(dom_element, vnode)
      # Skip Component vnodes - they manage their own events
      return if vnode.is_a?(VDOM::Component)
      return unless vnode.is_a?(VDOM::Element)

      event_types = []

      vnode.props.each do |key, value|
        key_str = key.to_s
        next unless key_str.start_with?('on')

        event_name = key_str[2..-1]&.downcase || ""
        event_types << event_name

        # addEventListener expects a block, not a Proc
        callback_id = case value
        when Symbol
          result = dom_element.addEventListener(event_name) do |event|
            begin
              self.send(value, event)
            rescue => e
              component_raised(e) if respond_to?(:component_raised)
              raise e
            end
          end
          result
        when Proc
          result = dom_element.addEventListener(event_name) do |event|
            begin
              # @type var value: Proc
              # Check if Proc expects arguments (arity)
              if value.arity == 0
                value.call
              else
                value.call(event)
              end
            rescue => e
              component_raised(e) if respond_to?(:component_raised)
              raise e
            end
          end
          result
        else
          raise "Invalid event handler: #{value.class}"
        end

        @event_listeners << callback_id
      end

      # Add debug attribute for DevTools
      if Funicular::Debug.enabled? && event_types.length > 0
        dom_element.setAttribute("data-event-listeners", event_types.join(","))
      end

      # Recursively bind events for children
      if vnode.children && dom_element.children
        children = dom_element.children.to_a
        vnode.children.each_with_index do |child_vnode, index|
          if child_vnode.is_a?(VDOM::Element)
            child_element = children[index]
            bind_events(child_element, child_vnode) if child_element
          elsif child_vnode.is_a?(VDOM::Component)
            # Component vnodes handle their own events in render_component
            # Skip them here to avoid duplicate event binding
          end
        end
      end
    end

    # Collect ref elements from VDOM
    # Called by VDOM::Renderer and Patcher
    def collect_refs(dom_element, vnode, refs_map = {})
      # Skip Component vnodes - they manage their own refs
      return refs_map if vnode.is_a?(VDOM::Component)
      return refs_map unless vnode.is_a?(VDOM::Element)

      if vnode.props[:ref]
        ref_name = vnode.props[:ref].to_sym
        @refs[ref_name] = dom_element
        refs_map[ref_name] = dom_element
      end

      if vnode.children && dom_element.children
        children = dom_element.children.to_a
        vnode.children.each_with_index do |child_vnode, index|
          if child_vnode.is_a?(VDOM::Element)
            child_element = children[index]
            collect_refs(child_element, child_vnode, refs_map) if child_element
          elsif child_vnode.is_a?(VDOM::Component)
            # Component vnodes handle their own refs in render_component
            # Skip them here to avoid duplicate processing
          end
        end
      end

      refs_map
    end

    # Cleanup event listeners
    # Called by VDOM::Patcher
    def cleanup_events
      @event_listeners.each do |callback_id|
        JS::Object.removeEventListener(callback_id)
      end
      @event_listeners = []

      # NOTE: Do NOT cleanup child component events here!
      # Child components manage their own events and will cleanup
      # when they themselves re-render or unmount
    end

    private

    # Normalize state value by converting JS::Object to Ruby native types
    def normalize_state_value(value)
      if value.is_a?(Hash)
        # Recursively normalize hash values
        normalized = {}
        value.each do |k, v|
          normalized[k] = normalize_state_value(v)
        end
        normalized
      elsif value.is_a?(Array)
        # Recursively normalize array elements
        value.map { |v| normalize_state_value(v) }
      elsif value.is_a?(JS::Object)
        # Convert JS::Object to appropriate Ruby type
        case value.type
        when :string
          value.to_s
        when :number
          # Check if it's an integer or float
          num = value.to_f
          num == num.to_i ? num.to_i : num
        when :boolean
          # JS::Object#== now supports direct comparison with Ruby true/false
          value == true
        when :null, :undefined
          nil
        when :array
          value.to_a.map { |v| normalize_state_value(v) }
        when :object
          # For plain objects, keep as JS::Object or convert to hash if needed
          value
        else
          value
        end
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
      collect_child_components(new_vdom)

      # Mark child components as mounted and call their lifecycle hooks
      @child_components.each do |child|
        unless child.mounted
          child.mounted = true
          child.component_mounted if child.respond_to?(:component_mounted)
        end
      end

      @vdom = new_vdom
    end

    # Add data-component attribute to the root element
    def add_data_component_attribute(vnode)
      return unless vnode.is_a?(VDOM::Element)
      vnode.props[:'data-component'] = self.class.to_s
      vnode.props[:'data-component-id'] = @__debug_id__.to_s if @__debug_id__
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
        VDOM::Text.new("")
      when Class
        # If it's a component class, create a component VNode
        if value.ancestors.include?(Funicular::Component)
          VDOM::Component.new(value, {})
        else
          VDOM::Text.new(value.to_s)
        end
      else
        VDOM::Text.new(value.to_s)
      end
    end

    # Collect child component instances from VDOM tree
    def collect_child_components(vnode)
      @child_components = []
      collect_child_components_recursive(vnode, @child_components)
    end

    def collect_child_components_recursive(vnode, components)
      if vnode.is_a?(VDOM::Component)
        components << vnode.instance if vnode.instance
        # Recursively collect from child component's vdom
        if vnode.instance && vnode.instance.vdom
          collect_child_components_recursive(vnode.instance.vdom, components)
        end
      elsif vnode.is_a?(VDOM::Element)
        vnode.children&.each do |child|
          # @type var child: VDOM::VNode
          collect_child_components_recursive(child, components)
        end
      end
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

        # Normalize props (convert StyleValue to String for :class)
        normalized_props = {}
        # @type var props: Hash[Symbol, String]
        props.each do |key, value|
          if key == :class && value.is_a?(StyleValue)
            normalized_props[key] = value.to_s
          else
            normalized_props[key] = value
          end
        end

        # @type var normalized_props: Hash[Symbol, String]
        element = VDOM::Element.new(tag, normalized_props, children)

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

    # Helper method to render child components in DSL
    # Accepts optional block for passing children to the component
    def component(component_class, props = {}, &block)
      unless component_class.is_a?(Class) && component_class.ancestors.include?(Funicular::Component)
        raise "component() expects a Funicular::Component class"
      end

      # If a block is provided, store the children builder in props
      # This allows components like ErrorBoundary to control child rendering
      if block
        props = props.merge(children_block: block)
      end

      vnode = VDOM::Component.new(component_class, props)

      # If we're inside another element's block, add this component to parent's children
      if @rendering && @current_children
        @current_children << vnode
      end

      vnode
    end

    # Rails-style form_for helper
    def form_for(model_key, options = {}, &block)
      on_submit = options.delete(:on_submit)
      form_class = options.delete(:class)

      # Build submit handler
      submit_handler = if on_submit
        ->(event) do
          event.preventDefault

          # Collect form data from state
          model_data = state.send(model_key)
          form_data = if model_data.is_a?(Hash)
            model_data
          elsif model_data.respond_to?(:instance_variables)
            data = {}
            model_data.instance_variables.each do |var|
              key = var.to_s.sub('@', '').to_sym
              data[key] = model_data.instance_variable_get(var)
            end
            data
          else
            {}
          end

          # Call the submit handler method
          send(on_submit, form_data)
        end
      else
        ->(event) { event.preventDefault }
      end

      # Render form
      form({ onsubmit: submit_handler, class: form_class }.merge(options)) do
        builder = Funicular::FormBuilder.new(self, model_key, options)
        block.call(builder)
      end
    end

    # Rails-style link_to helper
    # Default behavior: Fetch API for same-page actions (SPA-friendly)
    # Use navigate: true for router navigation (different component tree)
    def link_to(path, method: :get, navigate: false, **options, &block)
      if navigate
        # Navigation: Use <a> tag with real href for browser features
        # (right-click -> open in new tab, link preview, etc.)
        # Note: preventDefault is automatically called by js_add_event_listener
        # for <a> tags to enable SPA navigation
        merged_options = options.merge(href: path)
        merged_options[:onclick] = ->(event) {
          event.preventDefault  # Called for clarity, but already handled by JS layer
          handle_link_click(path)
        }
        a(merged_options, &block)
      else
        # Action: Use <div> tag (semantically more appropriate for actions)
        # No href needed, purely onclick-driven
        merged_options = options.merge(
          onclick: -> { handle_link_with_method(path, method) }
        )
        div(merged_options, &block)
      end
    end

    # Handle router navigation (navigate using History API)
    def handle_link_click(path)
      Funicular.router&.navigate(path)
    end

    # Handle link action via Fetch API
    def handle_link_with_method(path, method)
      # Call appropriate HTTP method
      case method.to_s.downcase.to_sym
      when :get
        HTTP.get(path) { |response| handle_link_response(response, path, method) }
      when :post
        HTTP.post(path) { |response| handle_link_response(response, path, method) }
      when :put
        HTTP.put(path) { |response| handle_link_response(response, path, method) }
      when :patch
        HTTP.patch(path) { |response| handle_link_response(response, path, method) }
      when :delete
        HTTP.delete(path) { |response| handle_link_response(response, path, method) }
      else
        raise "Unsupported HTTP method: #{method}"
      end
    end

    # Handle response from link action (can be overridden by subclasses)
    def handle_link_response(response, path, method)
      if response.error?
        puts "Link action failed (#{method.to_s.upcase} #{path}): #{response.error_message}"
      end
    end

    # Enable URL helpers from RouteHelpers module
    def method_missing(method, *args)
      if Funicular.const_defined?(:RouteHelpers)
        helpers = Funicular::RouteHelpers
        if helpers.instance_methods.include?(method)
          # Include helpers module and retry
          self.class.include(helpers) unless self.class.include?(helpers)
          return send(method, *args)
        end
      end
      super
    end

    def respond_to_missing?(method, include_private = false)
      if Funicular.const_defined?(:RouteHelpers)
        Funicular::RouteHelpers.instance_methods.include?(method) || super
      else
        super
      end
    end

    # Transition Helpers
    # These methods provide a declarative way to animate element removal/addition
    # using CSS transitions, inspired by Vue.js and Alpine.js transition systems

    # Remove an element via CSS transition animation
    #
    # @param element_id [String] DOM element ID (without '#' prefix)
    # @param from [String] CSS classes to remove before animation
    # @param to [String] CSS classes to add for leave animation
    # @param duration [Integer] Animation duration in milliseconds (default: 300)
    # @param callback [Proc] Block called after animation completes
    #
    # @example Remove message with fade out
    #   remove_via("message-123",
    #     "opacity-100 max-h-screen"
    #     "opacity-0 max-h-0",
    #     duration: 500,
    #   ) do
    #     patch(messages: updated_messages)
    #   end
    def remove_via(element_id, from, to, duration: 300, &callback)
      element = JS.document.getElementById(element_id)

      unless element
        callback.call if callback
        return
      end

      element.classList.remove(*from.split(" ")) unless from.empty?
      element.classList.add(*to.split(" ")) unless to.empty?

      JS.global.setTimeout(duration) do
        callback&.call
      end
    end

    # Add an element via CSS transition animation
    #
    # @param element_id [String] DOM element ID (without '#' prefix)
    # @param from [String] CSS classes to remove before animation
    # @param to [String] CSS classes to add for leave animation
    # @param duration [Integer] Animation duration in milliseconds (default: 300)
    # @param callback [Proc] Block called after animation completes
    #
    # @example Add message with fade in
    #   add_via("message-456",
    #     "opacity-0 scale-95",
    #     "opacity-100 scale-100",
    #     duration: 300
    #   )
    def add_via(element_id, from, to, duration: 300, &callback)
      element = JS.document.getElementById(element_id)

      unless element
        callback.call if callback
        return
      end

      element.classList.add(*from.split(" ")) unless from.empty?

      sleep_ms 10

      element.classList.remove(*from.split(" ")) unless from.empty?
      element.classList.add(*to.split(" ")) unless to.empty?

      JS.global.setTimeout(duration) do
        callback&.call
      end
    end
  end
end
