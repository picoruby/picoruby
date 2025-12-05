Rails.application.routes.draw do
  root "home#index"

  post "login", to: "sessions#create"
  delete "logout", to: "sessions#destroy"
  get "current_user", to: "sessions#show"

  resources :channels, only: [:index, :show]
  resources :users, only: [:show, :update] do
    member do
      get :avatar
    end
  end

  namespace :api do
    get "schema/user", to: "schema#user"
  end

  get "up" => "rails/health#show", as: :rails_health_check
end
