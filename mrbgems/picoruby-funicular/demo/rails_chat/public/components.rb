# LoginComponent - Login screen
class LoginComponent < Funicular::Component
  def initialize_state
    { username: "", password: "", error: nil, loading: false }
  end

  def handle_username_change(event)
    setState(username: event.target[:value])
  end

  def handle_password_change(event)
    setState(password: event.target[:value])
  end

  def handle_submit(event)
    event.preventDefault

    if @state[:username].empty? || @state[:password].empty?
      setState(error: "Please enter username and password")
      return
    end

    setState(loading: true, error: nil)

    # Login API call
    Funicular::HTTP.post("/login", {
      username: @state[:username],
      password: @state[:password]
    }) do |response|
      if response.error?
        setState(loading: false, error: response.error_message)
      else
        puts "Login successful: #{response.data['username']}"
        Funicular.router.navigate("/chat")
      end
    end
  end

  def render
    div(class: "min-h-screen flex items-center justify-center bg-gradient-to-br from-blue-500 to-purple-600") do
      div(class: "bg-white p-8 rounded-lg shadow-2xl w-96") do
        h1(class: "text-3xl font-bold text-center mb-8 text-gray-800") { "Funicular Chat" }

        if @state[:error]
          div(class: "bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded mb-4") do
            span { @state[:error] }
          end
        end

        form(onsubmit: :handle_submit, class: "space-y-4") do
          div do
            label(class: "block text-sm font-medium text-gray-700 mb-2") { "Username" }
            input(
              type: "text",
              value: @state[:username],
              oninput: :handle_username_change,
              class: "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500",
              placeholder: "Enter your username"
            )
          end

          div do
            label(class: "block text-sm font-medium text-gray-700 mb-2") { "Password" }
            input(
              type: "password",
              value: @state[:password],
              oninput: :handle_password_change,
              class: "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500",
              placeholder: "Enter your password"
            )
          end

          button(
            type: "submit",
            class: "w-full bg-blue-600 text-white py-2 px-4 rounded-md hover:bg-blue-700 transition duration-200 font-semibold #{'opacity-50 cursor-not-allowed' if @state[:loading]}"
          ) do
            span { @state[:loading] ? "Logging in..." : "Login" }
          end
        end

        div(class: "mt-6 text-center text-sm text-gray-600") do
          span { "Demo users: alice, bob, charlie (password: password)" }
        end
      end
    end
  end
end

# ChatComponent - Main chat interface
class ChatComponent < Funicular::Component
  def initialize_state
    {
      channels: [],
      current_channel: nil,
      messages: [],
      message_input: "",
      current_user: nil,
      loading: true
    }
  end

  def component_mounted
    # Check if logged in
    Funicular::HTTP.get("/current_user") do |response|
      if response.error?
        Funicular.router.navigate("/login")
      else
        setState(current_user: response.data)
        load_channels
      end
    end
  end

  def load_channels
    Funicular::HTTP.get("/channels") do |response|
      if response.error?
        setState(loading: false)
      else
        channels = response.data
        setState(channels: channels, loading: false)
        if channels.size > 0 && !@state[:current_channel]
          select_channel(channels[0])
        end
      end
    end
  end

  def select_channel(channel)
    setState(current_channel: channel, messages: [], loading: true)

    # Subscribe to ActionCable channel
    if @subscription
      @subscription.unsubscribe
    end

    consumer = Funicular::Cable.create_consumer("/cable")
    @subscription = consumer.subscriptions.create(
      { channel: "ChatChannel", channel_id: channel["id"] }
    ) do |data|
      case data["type"]
      when "initial_messages"
        setState(messages: data["messages"], loading: false)
        scroll_to_bottom
      when "new_message"
        messages = @state[:messages] + [data["message"]]
        setState(messages: messages)
        scroll_to_bottom
      end
    end
  end

  def scroll_to_bottom
    sleep 0.1
    if @refs[:messages_container]
      container = @refs[:messages_container]
      container[:scrollTop] = container[:scrollHeight]
    end
  end

  def handle_message_input(event)
    setState(message_input: event.target[:value])
  end

  def handle_send_message(event)
    event.preventDefault

    if @state[:message_input].empty?
      return
    end

    content = @state[:message_input]
    setState(message_input: "")

    @subscription.perform("send_message", { content: content })
  end

  def handle_logout(event)
    Funicular::HTTP.delete("/logout") do |response|
      Funicular.router.navigate("/login")
    end
  end

  def render
    div(class: "flex h-screen bg-gray-100") do
      # Sidebar - Channel list
      div(class: "w-64 bg-gray-800 text-white flex flex-col") do
        div(class: "p-4 bg-gray-900 flex justify-between items-center") do
          h2(class: "text-xl font-bold") { "Channels" }
          button(onclick: -> { Funicular.router.navigate("/settings") }, class: "text-gray-400 hover:text-white") do
            span { "⚙️" }
          end
        end

        div(class: "flex-1 overflow-y-auto") do
          @state[:channels].each do |channel|
            div(
              onclick: -> { select_channel(channel) },
              class: "p-4 hover:bg-gray-700 cursor-pointer #{@state[:current_channel] && @state[:current_channel]['id'] == channel['id'] ? 'bg-gray-700' : ''}"
            ) do
              div(class: "font-semibold") { "# #{channel['name']}" }
              div(class: "text-sm text-gray-400 truncate") { channel["description"] }
            end
          end
        end

        if @state[:current_user]
          div(class: "p-4 bg-gray-900 border-t border-gray-700") do
            div(class: "text-sm font-semibold") { @state[:current_user]["display_name"] }
            div(class: "text-xs text-gray-400") { "@#{@state[:current_user]['username']}" }
            button(onclick: :handle_logout, class: "mt-2 text-sm text-red-400 hover:text-red-300") do
              span { "Logout" }
            end
          end
        end
      end

      # Main chat area
      div(class: "flex-1 flex flex-col") do
        if @state[:current_channel]
          # Chat header
          div(class: "bg-white border-b border-gray-200 p-4") do
            h3(class: "text-xl font-bold text-gray-800") { "# #{@state[:current_channel]['name']}" }
            div(class: "text-sm text-gray-600") { @state[:current_channel]["description"] }
          end

          # Messages area
          div(ref: :messages_container, class: "flex-1 overflow-y-auto p-4 space-y-4") do
            if @state[:loading]
              div(class: "text-center text-gray-500") { "Loading messages..." }
            else
              @state[:messages].each do |message|
                div(class: "flex items-start space-x-3") do
                  # Avatar
                  if message["user"]["has_avatar"]
                    img(src: "/users/#{message['user']['id']}/avatar", class: "flex-shrink-0 w-10 h-10 rounded-full object-cover")
                  else
                    div(class: "flex-shrink-0 w-10 h-10 bg-blue-500 rounded-full flex items-center justify-center text-white font-bold") do
                      span { message["user"]["display_name"][0].upcase }
                    end
                  end
                  div(class: "flex-1") do
                    div(class: "flex items-baseline space-x-2") do
                      span(class: "font-semibold text-gray-900") { message["user"]["display_name"] }
                      span(class: "text-xs text-gray-500") { message["created_at"] }
                    end
                    div(class: "text-gray-800 mt-1") { message["content"] }
                  end
                end
              end
            end
          end

          # Message input
          div(class: "bg-white border-t border-gray-200 p-4") do
            form(onsubmit: :handle_send_message, class: "flex space-x-2") do
              input(
                type: "text",
                value: @state[:message_input],
                oninput: :handle_message_input,
                placeholder: "Type a message...",
                class: "flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
              )
              button(
                type: "submit",
                class: "px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 font-semibold"
              ) do
                span { "Send" }
              end
            end
          end
        else
          div(class: "flex-1 flex items-center justify-center text-gray-500") do
            span { "Select a channel to start chatting" }
          end
        end
      end
    end
  end
end

# SettingsComponent - User settings
class SettingsComponent < Funicular::Component
  def initialize_state
    { user: nil, display_name: "", message: nil, avatar_preview: nil, saving: false }
  end

  def component_mounted
    Funicular::HTTP.get("/current_user") do |response|
      if response.error?
        Funicular.router.navigate("/login")
      else
        # Create User model instance from response data
        user = User.new(response.data)
        setState(user: user, display_name: user.display_name)
      end
    end
  end

  def handle_display_name_change(event)
    setState(display_name: event.target[:value])
  end

  def handle_avatar_change(event)
    Funicular::FileUpload.select_file_with_preview('avatar-input') do |file, preview_url|
      if file && preview_url
        @selected_avatar_file = file
        setState(avatar_preview: preview_url)
      else
        @selected_avatar_file = nil
        setState(avatar_preview: nil)
      end
    end
  end

  def handle_save(event)
    event.preventDefault
    setState(saving: true, message: nil)

    if @selected_avatar_file
      # If avatar file exists, use FormData to upload both display_name and avatar
      save_with_formdata
    else
      # If no avatar file, use Model layer to update display_name only
      save_with_model
    end
  end

  def save_with_model
    @state[:user].display_name = @state[:display_name]
    @state[:user].update do |success, result|
      if success
        setState(
          saving: false,
          message: "Settings saved successfully!",
          user: User.new(result)
        )
      else
        setState(saving: false, message: "Error: #{result}")
      end
    end
  end

  def save_with_formdata
    url = "/users/#{@state[:user].id}"
    fields = { display_name: @state[:display_name] }

    Funicular::FileUpload.upload_with_formdata(
      url,
      fields: fields,
      file_field: 'avatar',
      file: @selected_avatar_file
    ) do |result|
      @selected_avatar_file = nil
      handle_formdata_response(result)
    end
  end

  def handle_formdata_response(result)
    if result.nil?
      setState(saving: false, message: "Failed to parse response")
    elsif result["error"] || result["errors"]
      setState(saving: false, message: "Failed to save settings")
    else
      # Update user instance with new data
      updated_user = @state[:user]
      updated_user.instance_variable_set("@display_name", result["display_name"])
      if result["avatar_updated"]
        updated_user.instance_variable_set("@has_avatar", true)
      end

      setState(
        saving: false,
        message: "Settings saved successfully!",
        user: updated_user,
        avatar_preview: nil
      )
    end
  end

  def render
    div(class: "min-h-screen bg-gray-100 py-8") do
      div(class: "max-w-2xl mx-auto bg-white rounded-lg shadow-md p-8") do
        div(class: "flex items-center justify-between mb-6") do
          h1(class: "text-2xl font-bold text-gray-800") { "Settings" }
          button(
            onclick: -> { Funicular.router.navigate("/chat") },
            class: "text-blue-600 hover:text-blue-800"
          ) do
            span { "← Back to Chat" }
          end
        end

        if @state[:message]
          div(class: "mb-4 p-4 bg-green-100 border border-green-400 text-green-700 rounded") do
            span { @state[:message] }
          end
        end

        if @state[:user]
          form(onsubmit: :handle_save, class: "space-y-6") do
            div do
              label(class: "block text-sm font-medium text-gray-700 mb-2") { "Username" }
              input(
                type: "text",
                value: @state[:user].username,
                disabled: true,
                class: "w-full px-3 py-2 border border-gray-300 rounded-md bg-gray-100 text-gray-600"
              )
            end

            div do
              label(class: "block text-sm font-medium text-gray-700 mb-2") { "Display Name" }
              input(
                type: "text",
                value: @state[:display_name],
                oninput: :handle_display_name_change,
                class: "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              )
            end

            div do
              label(class: "block text-sm font-medium text-gray-700 mb-2") { "Avatar" }
              if @state[:avatar_preview]
                div(class: "mb-2") do
                  img(src: @state[:avatar_preview], class: "w-24 h-24 rounded-full object-cover")
                end
              elsif @state[:user].has_avatar
                div(class: "mb-2") do
                  img(src: "/users/#{@state[:user].id}/avatar", class: "w-24 h-24 rounded-full object-cover")
                end
              end
              input(
                id: "avatar-input",
                type: "file",
                accept: "image/*",
                onchange: :handle_avatar_change,
                class: "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              )
            end

            button(
              type: "submit",
              class: "w-full bg-blue-600 text-white py-2 px-4 rounded-md hover:bg-blue-700 transition duration-200 font-semibold #{@state[:saving] ? 'opacity-50 cursor-not-allowed' : ''}"
            ) do
              span { @state[:saving] ? "Saving..." : "Save Changes" }
            end
          end
        end
      end
    end
  end
end
