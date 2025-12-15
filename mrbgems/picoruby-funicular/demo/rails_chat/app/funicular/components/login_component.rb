class LoginComponent < Funicular::Component
  styles do
    container "min-h-screen flex items-center justify-center bg-gradient-to-br from-blue-500 to-purple-600"
    card "bg-white p-8 rounded-lg shadow-2xl w-96"
    title "text-3xl font-bold text-center mb-8 text-gray-800"
    error_box "bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded mb-4"
    form "space-y-4"
    label "block text-sm font-medium text-gray-700 mb-2"
    input "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"

    button base: "w-full py-2 px-4 rounded-md transition duration-200 font-semibold",
           variants: {
             normal: "bg-blue-600 text-white hover:bg-blue-700",
             loading: "bg-blue-600 text-white hover:bg-blue-700 opacity-50 cursor-not-allowed"
           }

    hint "mt-6 text-center text-sm text-gray-600"
  end

  def initialize_state
    { username: "", password: "", error: nil, loading: false }
  end

  def handle_username_change(event)
    patch(username: event.target[:value])
  end

  def handle_password_change(event)
    patch(password: event.target[:value])
  end

  def handle_submit(event)
    event.preventDefault

    if state.username.empty? || state.password.empty?
      patch(error: "Please enter username and password")
      return
    end

    patch(loading: true, error: nil)

    # Login using Session model
    Session.login(state.username, state.password) do |user, error|
      if error
        patch(loading: false, error: error)
      else
        puts "Login successful: #{user.username}"
        Funicular.router.navigate("/chat")
      end
    end
  end

  def render
    div(class: s.container) do
      div(class: s.card) do
        h1(class: s.title) { "Funicular Chat" }

        if state.error
          div(class: s.error_box) do
            span { state.error }
          end
        end

        form(onsubmit: :handle_submit, class: s.form) do
          div do
            label(class: s.label) { "Username" }
            input(
              type: "text",
              value: state.username,
              oninput: :handle_username_change,
              class: s.input,
              placeholder: "Enter your username"
            )
          end

          div do
            label(class: s.label) { "Password" }
            input(
              type: "password",
              value: state.password,
              oninput: :handle_password_change,
              class: s.input,
              placeholder: "Enter your password"
            )
          end

          button(
            type: "submit",
            class: s.button(state.loading ? :loading : :normal)
          ) do
            span { state.loading ? "Logging in..." : "Login" }
          end
        end

        div(class: s.hint) do
          span { "Demo users: alice, bob, charlie (password: password)" }
        end
      end
    end
  end
end

