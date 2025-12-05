class UsersController < ApplicationController
  before_action :require_login
  skip_before_action :verify_authenticity_token, only: [:update]

  def show
    user = User.find(params[:id])
    render json: {
      id: user.id,
      username: user.username,
      display_name: user.display_name,
      has_avatar: user.avatar.present?
    }
  rescue ActiveRecord::RecordNotFound
    render json: { error: "User not found" }, status: :not_found
  end

  def avatar
    user = User.find(params[:id])
    if user.avatar
      send_data user.avatar, type: 'image/png', disposition: 'inline'
    else
      head :not_found
    end
  rescue ActiveRecord::RecordNotFound
    head :not_found
  end

  def update
    unless current_user.id == params[:id].to_i
      return render json: { error: "Unauthorized" }, status: :unauthorized
    end

    if params[:display_name]
      current_user.display_name = params[:display_name]
    end

    if params[:avatar]
      # params[:avatar] is now an ActionDispatch::Http::UploadedFile object
      current_user.avatar = params[:avatar].read
    end

    if current_user.save
      render json: {
        id: current_user.id,
        username: current_user.username,
        display_name: current_user.display_name,
        avatar_updated: params[:avatar].present?
      }
    else
      render json: { errors: current_user.errors.full_messages }, status: :unprocessable_entity
    end
  end
end
