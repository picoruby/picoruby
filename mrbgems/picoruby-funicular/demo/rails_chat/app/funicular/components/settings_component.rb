class SettingsComponent < Funicular::Component
  styles do
    container "min-h-screen bg-gray-100 py-8"
    card "max-w-2xl mx-auto bg-white rounded-lg shadow-md p-8"
    header "flex items-center justify-between mb-6"
    title "text-2xl font-bold text-gray-800"
    back_button "text-blue-600 hover:text-blue-800"
    message "mb-4 p-4 border rounded bg-green-100 border-green-400 text-green-700"
    form "space-y-6"
    label "block text-sm font-medium text-gray-700 mb-2"
    input "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
    input_disabled "w-full px-3 py-2 border border-gray-300 rounded-md bg-gray-100 text-gray-600"
    avatar_container "mb-2"
    avatar "w-24 h-24 rounded-full object-cover"

    submit_button base: "w-full py-2 px-4 rounded-md transition duration-200 font-semibold",
                  variants: {
                    normal: "bg-blue-600 text-white hover:bg-blue-700",
                    saving: "bg-blue-600 text-white hover:bg-blue-700 opacity-50 cursor-not-allowed"
                  }
  end

  def initialize_state
    {
      user: { username: "", display_name: "" },
      user_object: nil,
      errors: {},
      message: nil,
      avatar_preview: nil,
      saving: false,
      avatar_cache_buster: Time.now.to_i
    }
  end

  def component_mounted
    # Get current user using Session model
    Session.current_user do |user, error|
      if error
        Funicular.router.navigate("/login")
      else
        patch(
          user_object: user,
          user: {
            username: user.username,
            display_name: user.display_name
          }
        )
      end
    end
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

  def handle_save(data)
    patch(saving: true, message: nil, errors: {})

    if @selected_avatar_file
      # If avatar file exists, use FormData to upload both display_name and avatar
      save_with_formdata(data[:display_name])
    else
      # If no avatar file, use Model layer to update display_name only
      save_with_model(data[:display_name])
    end
  end

  def save_with_model(display_name)
    # Preserve has_avatar state
    had_avatar = state.user_object.has_avatar

    state.user_object.display_name = display_name
    state.user_object.update do |success, result|
      if success
        updated_user_object = User.new(result)
        # Preserve has_avatar if not included in response
        if result["has_avatar"].nil? && had_avatar
          updated_user_object.instance_variable_set("@has_avatar", true)
        end

        patch(
          saving: false,
          message: "Settings saved successfully!",
          user_object: updated_user_object,
          user: {
            username: updated_user_object.username,
            display_name: updated_user_object.display_name
          }
        )
      else
        patch(saving: false, message: "Error: #{result}")
      end
    end
  end

  def save_with_formdata(display_name)
    url = "/users/#{state.user_object.id}"
    fields = { display_name: display_name }

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
      updated_user_object = state.user_object
      updated_user_object.instance_variable_set("@display_name", result["display_name"])
      if result["avatar_updated"]
        updated_user_object.instance_variable_set("@has_avatar", true)
      end

      patch(
        saving: false,
        message: "Settings saved successfully!",
        user_object: updated_user_object,
        user: {
          username: updated_user_object.username,
          display_name: result["display_name"]
        },
        avatar_preview: nil,
        avatar_cache_buster: Time.now.to_i
      )
    end
  end

  def render
    div(class: s.container) do
      div(class: s.card) do
        div(class: s.header) do
          h1(class: s.title) { "Settings" }
          button(
            onclick: -> { Funicular.router.navigate("/chat") },
            class: s.back_button
          ) do
            span { "← Back to Chat" }
          end
        end

        if state.message
          div(class: s.message) do
            span { state.message }
          end
        end

        if state.user_object
          form_for(:user, on_submit: :handle_save, class: s.form) do |f|
            div do
              f.label :username
              f.text_field :username, disabled: true, class: s.input_disabled
            end

            div do
              f.label :display_name, "Display Name"
              f.text_field :display_name, class: s.input
            end

            div do
              label(class: s.label) { "Avatar" }
              if state.avatar_preview
                div(class: s.avatar_container) do
                  img(src: state.avatar_preview, class: s.avatar)
                end
              elsif state.user_object.has_avatar
                div(class: s.avatar_container) do
                  img(src: "/users/#{state.user_object.id}/avatar?t=#{state.avatar_cache_buster}", class: s.avatar)
                end
              end
              f.file_field :avatar, id: "avatar-input", accept: "image/*", onchange: :handle_avatar_change, class: s.input
            end

            f.submit(
              state.saving ? "Saving..." : "Save Changes",
              class: s.submit_button(state.saving ? :saving : :normal)
            )
          end
        end
      end
    end
  end
end

