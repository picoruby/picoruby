module Funicular
  # ErrorBoundary component catches errors from child components and displays
  # a fallback UI instead of crashing the entire application.
  #
  # Usage:
  #   component(ErrorBoundary) do
  #     component(RiskyComponent)
  #   end
  #
  # With custom fallback:
  #   component(ErrorBoundary, fallback: ->(error) { div { "Error: #{error.message}" } }) do
  #     component(RiskyComponent)
  #   end
  #
  # Props:
  #   - fallback: Proc or Method that receives the error and returns VDOM
  #   - on_error: Optional callback when error is caught (for logging, reporting)
  #
  class ErrorBoundary < Component
    def initialize_state
      { has_error: false, error: nil, error_info: nil }
    end

    # Called when a child component raises an error during rendering
    # Returns true to indicate the error was handled and should not propagate
    def catch_error(error, error_info = nil)
      # Update state to show fallback UI
      @state[:has_error] = true
      @state[:error] = error
      @state[:error_info] = error_info
      @state_accessor = nil

      # Mark that we caught an error (used to prevent @vdom overwrite)
      @error_caught_during_render = true

      # Call on_error callback if provided
      if props[:on_error]
        begin
          props[:on_error].call(error, error_info)
        rescue => callback_error
          puts "[ErrorBoundary] on_error callback failed: #{callback_error.message}"
        end
      end

      # Report to debug module
      Funicular::Debug.report_error(self, error, error_info) if Funicular::Debug.enabled?

      true # Indicate error was handled
    end

    # Reset the error boundary to try rendering children again
    def reset
      begin
        patch(has_error: false, error: nil, error_info: nil)
      rescue => e
        # Re-catch the error and show fallback again
        catch_error(e, { component_class: "child component" })
      end
    end

    def render
      if state[:has_error]
        render_fallback
      else
        render_children
      end
    end

    private

    def render_fallback
      if props[:fallback]
        result = props[:fallback].call(state[:error])
        if result.is_a?(VDOM::VNode)
          result
        else
          div { result.to_s }
        end
      else
        default_fallback
      end
    end

    def default_fallback
      div(class: 'error-boundary-fallback', style: 'padding: 20px; background: #fee; border: 1px solid #f00; border-radius: 4px;') do
        h3(style: 'color: #c00; margin: 0 0 10px 0;') { "Something went wrong" }
        if state[:error]
          div(style: 'font-family: monospace; white-space: pre-wrap; font-size: 12px; color: #600;') do
            "#{state[:error].class}: #{state[:error].message}"
          end
        end
        if Funicular.env.development? && state[:error_info]
          div(style: 'margin-top: 10px; font-size: 11px; color: #666;') do
            "Component: #{state[:error_info][:component_class]}"
          end
        end
      end
    end

    def render_children
      # Children are passed via children_block prop from the component() DSL
      # Errors are caught by VDOM::Renderer.render_component
      if props[:children_block]
        div(class: 'error-boundary-content') do
          props[:children_block].call
        end
      elsif props[:children]
        props[:children]
      else
        div { "" }
      end
    end
  end

  # Make ErrorBoundary available at top level
  def self.const_missing(name)
    if name == :ErrorBoundary
      Funicular::ErrorBoundary
    else
      super
    end
  end
end
