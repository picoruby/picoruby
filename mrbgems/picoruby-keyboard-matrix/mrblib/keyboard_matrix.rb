class KeyboardMatrix
  # Start scanning loop with callback
  def start(&block)
    @callback = block
    unless @callback
      raise "Callback block is required. Use on_key_event to set a callback."
    end
    # @type ivar @callback: Proc
    loop do
      event = scan
      if event
        @callback.call(event)
      end
      sleep_ms(1)
    end
  end

  # Set event callback
  def on_key_event(&block)
    @callback = block
  end
end
