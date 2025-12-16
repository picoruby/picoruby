class ChatComponent < Funicular::Component
  styles do
    layout "flex h-screen bg-gray-100"
    main_content "flex-1 flex"
  end

  def initialize(params = {})
    super
    @requested_channel_id = params[:channel_id]&.to_i
  end

  def initialize_state
    {
      channels: [],
      current_channel: nil,
      messages: [],
      message_input: "",
      current_user: nil,
      loading: true,
      stats: []
    }
  end

  def component_mounted
    # Check if logged in using Session model
    Session.current_user do |user, error|
      if error
        Funicular.router.navigate("/login")
      else
        patch(current_user: user)
        load_channels
      end
    end
  end

  def component_will_unmount
    @subscription.unsubscribe if @subscription
    @stats_subscription.unsubscribe if @stats_subscription
  end

  def load_channels
    # Load channels using Channel model
    Channel.all do |channels, error|
      if error
        patch(loading: false)
      else
        patch(channels: channels, loading: false)
        if channels.size > 0 && !state.current_channel
          # Select requested channel if specified, otherwise select first channel
          if @requested_channel_id
            selected_channel = channels.find { |ch| ch.id == @requested_channel_id }
            select_channel(selected_channel) if selected_channel
          else
            select_channel(channels[0])
          end
        end
      end
    end
  end

  def select_channel(channel)
    patch(current_channel: channel, messages: [], loading: true, stats: [])

    # Update URL to reflect the current channel
    new_path = "/chat/#{channel.id}"
    if Funicular.router.current_location_path != new_path
      JS.global.history.replaceState(JS::Bridge.to_js({}), '', new_path)
    end

    # Subscribe to ActionCable channel
    if @subscription
      @subscription.unsubscribe
    end
    if @stats_subscription
      @stats_subscription.unsubscribe
    end

    consumer = Funicular::Cable.create_consumer("/cable")
    @subscription = consumer.subscriptions.create(
      { channel: "ChatChannel", channel_id: channel.id }
    ) do |data|
      case data["type"]
      when "initial_messages"
        patch(messages: data["messages"], loading: false)
      when "new_message"
        messages = state.messages + [data["message"]]
        patch(messages: messages)
      end
    end

    # Subscribe to StatsChannel
    @stats_subscription = consumer.subscriptions.create(
      { channel: "StatsChannel", channel_id: channel.id }
    ) do |data|
      case data["type"]
      when "stats_update"
        patch(stats: data["stats"])
      end
    end
  end

  def handle_message_input(event)
    patch(message_input: event.target[:value])
  end

  def handle_send_message(event)
    event.preventDefault

    if state.message_input.empty?
      return
    end

    content = state.message_input

    # Clear the form
    form = event[:target]
    form.reset if form

    # Clear state
    patch(message_input: "")

    @subscription.perform("send_message", { content: content })
  end

  def handle_logout(event)
    # Logout using Session model
    Session.logout do |success, error|
      Funicular.router.navigate("/login")
    end
  end

  def render
    div(class: s.layout) do
      # Sidebar - Channel list
      component(ChannelListComponent, {
        channels: state.channels,
        current_channel: state.current_channel,
        current_user: state.current_user,
        on_select_channel: ->(channel) { select_channel(channel) },
        on_logout: ->(event) { handle_logout(event) }
      })

      # Main content area (chat + stats)
      div(class: s.main_content) do
        # Chat area
        component(MessageListComponent, {
          current_channel: state.current_channel,
          messages: state.messages,
          loading: state.loading,
          message_input: state.message_input,
          on_message_input: ->(event) { handle_message_input(event) },
          on_send_message: ->(event) { handle_send_message(event) },
          preserve: true
        })

        # Stats chart area
        if state.current_channel
          component(StatsChartComponent, {
            stats: state.stats
          })
        end
      end
    end
  end
end
