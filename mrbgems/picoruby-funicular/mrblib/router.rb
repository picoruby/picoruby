module Funicular
  class Router
    attr_reader :routes, :current_component, :current_path

    def initialize(container)
      @container = container
      @routes = []
      @default_route = nil
      @current_component = nil
      @current_path = nil
      @popstate_callback_id = nil
      @url_helpers = Module.new
      Funicular.const_set(:RouteHelpers, @url_helpers) unless Funicular.const_defined?(:RouteHelpers)
    end

    # Rails-style DSL methods
    def get(path, to:, as: nil)
      add_route_with_method(:get, path, to, as)
    end

    def post(path, to:, as: nil)
      add_route_with_method(:post, path, to, as)
    end

    def put(path, to:, as: nil)
      add_route_with_method(:put, path, to, as)
    end

    def patch(path, to:, as: nil)
      add_route_with_method(:patch, path, to, as)
    end

    def delete(path, to:, as: nil)
      add_route_with_method(:delete, path, to, as)
    end

    # Add a route (backward compatibility)
    def add_route(path, component_class, as: nil)
      add_route_with_method(:get, path, component_class, as)
    end

    # Set default route (used when path is empty)
    def set_default(path)
      @default_route = path
    end

    # Start listening to popstate
    def start
      # Clean up existing listener if any (prevents duplicate registration)
      if @popstate_callback_id
        JS::Object.removeEventListener(@popstate_callback_id)
        @popstate_callback_id = nil
      end

      # Set up popstate listener
      @popstate_callback_id = JS.global.addEventListener('popstate') do |event|
        handle_route_change
      end

      # Handle initial route
      if current_location_path == '/' && @default_route
        # Use replaceState to not add a new entry to the history
        JS.global.history.replaceState(JS::Bridge.to_js({}), '', @default_route)
      end
      handle_route_change
    end

    # Stop listening to popstate
    def stop
      if @popstate_callback_id
        JS::Object.removeEventListener(@popstate_callback_id)
        @popstate_callback_id = nil
      end

      unmount_current_component
    end

    # Navigate to a path programmatically using History API
    def navigate(path)
      JS.global.history.pushState(JS::Bridge.to_js({}), '', path)
      # Manually trigger route change because pushState doesn't fire popstate
      handle_route_change
    end

    # Get current path from location
    def current_location_path
      js_path_obj = JS.global.location.pathname
      path = js_path_obj.to_s
      path.empty? ? '/' : path
    end

    private

    # Handle route change
    def handle_route_change
      path = current_location_path

      # Find matching route
      component_class, params = find_route(path)

      unless component_class
        # Maybe render a 404 component?
        return
      end

      # Don't remount if already on this path
      return if @current_path == path

      # Unmount current component
      unmount_current_component

      # Mount new component
      @current_path = path
      @current_component = component_class.new(params)
      # @type ivar @current_component: Funicular::Component
      @current_component.mount(@container)
    end

    # Unmount current component
    def unmount_current_component
      @current_component&.unmount
      @current_component = nil
      @current_path = nil
    end

    private

    def add_route_with_method(method, path, component_class, name = '')
      pattern_segments = path.split('/').reject { |s| s.empty? }
      route = {
        method: method,
        path: path,
        component: component_class,
        name: name,
        pattern_segments: pattern_segments
      }
      # @type var route: Funicular::route_definition_t
      @routes << route

      # Generate URL helper if name is provided
      generate_url_helper(name, path) if name
    end

    def generate_url_helper(name, path_pattern)
      helper_method_name = "#{name}_path".to_sym

      # Check for duplicate helper names
      if @url_helpers.instance_methods.include?(helper_method_name)
        raise "URL helper '#{helper_method_name}' is already defined"
      end

      # Extract parameter names from path pattern (without regex)
      param_names = extract_param_names(path_pattern)

      # Define the helper method
      if param_names.empty?
        # No parameters - return static path
        @url_helpers.module_eval do
          define_method(helper_method_name) do # steep:ignore
            path_pattern
          end
        end
      else
        # With parameters
        @url_helpers.module_eval do
          define_method(helper_method_name) do |*args| # steep:ignore
            # Handle model objects with id method
            if args.length == 1 && args[0].respond_to?(:id) && param_names.length == 1
              args = [args[0].id]
            elsif args.length != param_names.length
              raise ArgumentError, "#{helper_method_name} expects #{param_names.length} argument(s), got #{args.length}"
            end

            result = path_pattern.dup
            param_names.each_with_index do |param, idx|
              result = result.sub(":#{param}", args[idx].to_s)
            end
            result
          end
        end
      end
    end

    def extract_param_names(path_pattern)
      param_names = []
      i = 0
      while i < path_pattern.length
        if path_pattern[i] == ':'
          # Found parameter marker, extract param name
          i += 1
          param_name = ""
          while i < path_pattern.length
            char = path_pattern[i] || ''
            # Check if char is alphanumeric or underscore
            if (char >= 'a' && char <= 'z') || (char >= 'A' && char <= 'Z') || (char >= '0' && char <= '9') || char == '_'
              param_name += char
              i += 1
            else
              break
            end
          end
          param_names << param_name.to_sym unless param_name.empty?
        else
          i += 1
        end
      end
      param_names
    end

    def find_route(path)
      path_segments = path.split('/').reject { |s| s.empty? }
      params = {}

      @routes.each do |route|
        pattern_segments = route[:pattern_segments]
        next if pattern_segments.length != path_segments.length

        match = true

        pattern_segments.each_with_index do |pattern_segment, index|
          path_segment = path_segments[index]

          if pattern_segment.start_with?(':')
            param_name = pattern_segment[1..-1]&.to_sym
            if param_name.nil?
              raise "Invalid parameter name in route pattern: #{route[:path]}"
            end
            params[param_name] = path_segment
          elsif pattern_segment != path_segment
            match = false
            break
          end
        end

        return [route[:component], params] if match
      end

      [nil, params] # No route found
    end
  end
end
