class ChannelsController < ApplicationController
  before_action :require_login

  def index
    channels = Channel.order(:name)
    render json: channels.map { |c|
      {
        id: c.id,
        name: c.name,
        description: c.description
      }
    }
  end

  def show
    channel = Channel.find(params[:id])
    messages = channel.messages.includes(:user).order(created_at: :desc).limit(100)

    render json: {
      id: channel.id,
      name: channel.name,
      description: channel.description,
      messages: messages.reverse.map { |m|
        {
          id: m.id,
          content: m.content,
          created_at: m.created_at.iso8601,
          user: {
            id: m.user.id,
            username: m.user.username,
            display_name: m.user.display_name,
            has_avatar: m.user.avatar.present?
          }
        }
      }
    }
  rescue ActiveRecord::RecordNotFound
    render json: { error: "Channel not found" }, status: :not_found
  end
end
