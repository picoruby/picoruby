class Api::SchemaController < ApplicationController
  def user
    render json: {
      attributes: {
        "id" => { type: "integer", readonly: true },
        "username" => { type: "string", readonly: true },
        "display_name" => { type: "string", editable: true },
        "has_avatar" => { type: "boolean", readonly: true },
        "avatar" => {
          type: "binary",
          editable: false,
          upload_endpoint: "/users/:id/avatar"
        }
      },
      endpoints: {
        "find" => { method: "GET", path: "/users/:id" },
        "update" => { method: "PATCH", path: "/users/:id" }
      }
    }
  end
end
