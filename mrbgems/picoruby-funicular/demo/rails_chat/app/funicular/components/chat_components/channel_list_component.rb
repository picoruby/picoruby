class ChannelListComponent < Funicular::Component
  styles do
    sidebar "w-64 bg-gray-800 text-white flex flex-col"

    sidebar_header "p-4 bg-gray-900 flex justify-between items-center"
    sidebar_title "text-xl font-bold"
    settings_button "text-gray-400 hover:text-white cursor-pointer"
    channels_list "flex-1 overflow-y-auto"

    channel_item base: "p-4 hover:bg-gray-700 cursor-pointer",
                 active: "bg-gray-700"
    channel_name "font-semibold"
    channel_desc "text-sm text-gray-400 truncate"

    user_info "p-4 bg-gray-900 border-t border-gray-700"
    user_name "text-sm font-semibold"
    user_handle "text-xs text-gray-400"
    logout_button "mt-2 text-sm text-red-400 hover:text-red-300 cursor-pointer"
  end

  def render
    div(class: s.sidebar) do
      div(class: s.sidebar_header) do
        h2(class: s.sidebar_title) { "Channels" }
        link_to settings_path, navigate: true, class: s.settings_button do
          span { "⚙️" }
        end
      end

      div(class: s.channels_list) do
        props[:channels].each do |channel|
          is_active = props[:current_channel] && props[:current_channel].id == channel.id
          div(key: channel.id, class: s.channel_item(is_active), onclick: -> { props[:on_select_channel].call(channel) }) do
            div(class: s.channel_name) { "# #{channel.name}" }
            div(class: s.channel_desc) { channel.description }
          end
        end
      end

      if props[:current_user]
        div(class: s.user_info) do
          div(class: s.user_name) { props[:current_user].display_name }
          div(class: s.user_handle) { "@#{props[:current_user].username}" }
          button(onclick: props[:on_logout], class: s.logout_button) do
            span { "Logout" }
          end
        end
      end
    end
  end
end
