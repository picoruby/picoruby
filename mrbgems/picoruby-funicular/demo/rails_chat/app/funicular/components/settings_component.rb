class SettingsComponent < Funicular::Component
  def initialize_state
    { user: nil, display_name: "", message: nil, avatar_preview: nil, saving: false, avatar_cache_buster: Time.now.to_i }
  end

  def component_mounted
    # Get current user using Session model
    Session.current_user do |user, error|
      if error
        Funicular.router.navigate("/login")
      else
        patch(user: user, display_name: user.display_name)
      end
    end
  end

  def handle_display_name_change(event)
    patch(display_name: event.target[:value])
  end

  def handle_avatar_change(event)
    Funicular::FileUpload.select_file_with_preview('avatar-input') do |file, preview_url|
      if file && preview_url
        @selected_avatar_file = file
        patch(avatar_preview: preview_url)
      else
        @selected_avatar_file = nil
        patch(avatar_preview: nil)
      end
    end
  end

  def handle_save(event)
    event.preventDefault
    patch(saving: true, message: nil)

    if @selected_avatar_file
      # If avatar file exists, use FormData to upload both display_name and avatar
      save_with_formdata
    else
      # If no avatar file, use Model layer to update display_name only
      save_with_model
    end
  end

  def save_with_model
    state.user.display_name = state.display_name
    state.user.update do |success, result|
      if success
        patch(
          saving: false,
          message: "Settings saved successfully!",
          user: User.new(result)
        )
      else
        patch(saving: false, message: "Error: #{result}")
      end
    end
  end

  def save_with_formdata
    url = "/users/#{state.user.id}"
    fields = { display_name: state.display_name }

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
      patch(saving: false, message: "Failed to parse response")
    elsif result["error"] || result["errors"]
      patch(saving: false, message: "Failed to save settings")
    else
      # Update user instance with new data
      updated_user = state.user
      updated_user.instance_variable_set("@display_name", result["display_name"])
      if result["avatar_updated"]
        updated_user.instance_variable_set("@has_avatar", true)
      end

      patch(
        saving: false,
        message: "Settings saved successfully!",
        user: updated_user,
        avatar_preview: nil,
        avatar_cache_buster: Time.now.to_i
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

        if state.message
          div(class: "mb-4 p-4 bg-green-100 border border-green-400 text-green-700 rounded") do
            span { state.message }
          end
        end

        if state.user
          form(onsubmit: :handle_save, class: "space-y-6") do
            div do
              label(class: "block text-sm font-medium text-gray-700 mb-2") { "Username" }
              input(
                type: "text",
                value: state.user.username,
                disabled: true,
                class: "w-full px-3 py-2 border border-gray-300 rounded-md bg-gray-100 text-gray-600"
              )
            end

            div do
              label(class: "block text-sm font-medium text-gray-700 mb-2") { "Display Name" }
              input(
                type: "text",
                value: state.display_name,
                oninput: :handle_display_name_change,
                class: "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              )
            end

            div do
              label(class: "block text-sm font-medium text-gray-700 mb-2") { "Avatar" }
              if state.avatar_preview
                div(class: "mb-2") do
                  img(src: state.avatar_preview, class: "w-24 h-24 rounded-full object-cover")
                end
              elsif state.user.has_avatar
                div(class: "mb-2") do
                  img(src: "/users/#{state.user.id}/avatar?t=#{state.avatar_cache_buster}", class: "w-24 h-24 rounded-full object-cover")
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
              class: "w-full bg-blue-600 text-white py-2 px-4 rounded-md hover:bg-blue-700 transition duration-200 font-semibold #{state.saving ? 'opacity-50 cursor-not-allowed' : ''}"
            ) do
              span { state.saving ? "Saving..." : "Save Changes" }
            end
          end
        end
      end
    end
  end
end

