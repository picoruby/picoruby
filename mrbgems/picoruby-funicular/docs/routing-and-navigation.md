# Routing and Navigation

This guide covers Funicular's built-in client-side routing and the Rails-style `link_to` helper for navigation.

## Table of Contents

- [Router Setup](#router-setup)
- [Defining Routes](#defining-routes)
- [URL Parameters](#url-parameters)
- [Named Routes and URL Helpers](#named-routes-and-url-helpers)
- [Programmatic Navigation](#programmatic-navigation)
- [link_to Helper](#link_to-helper)
- [Best Practices](#best-practices)

## Router Setup

Initialize the router when starting your application:

```ruby
Funicular.start(container: 'app') do |router|
  router.get('/login', to: LoginComponent, as: 'login')
  router.get('/dashboard', to: DashboardComponent, as: 'dashboard')
  router.get('/settings', to: SettingsComponent, as: 'settings')

  router.set_default('/login')  # Redirect / to /login
end
```

The router:
- Uses the History API for SPA navigation
- Listens to browser back/forward buttons
- Automatically mounts/unmounts components on route changes

## Defining Routes

### Basic Routes

```ruby
router.get('/about', to: AboutComponent, as: 'about')
router.get('/contact', to: ContactComponent, as: 'contact')
```

### HTTP Method Routes

While Funicular is client-side, you can define method-specific routes for semantic clarity:

```ruby
router.get('/posts', to: PostListComponent, as: 'posts')
router.post('/posts', to: CreatePostComponent, as: 'create_post')
router.delete('/posts/:id', to: DeletePostComponent, as: 'delete_post')
```

**Note**: These routes are for client-side organization. For server communication, use the `link_to` helper with `method:` option (see below).

## URL Parameters

Define dynamic routes with `:param` syntax:

```ruby
router.get('/users/:id', to: UserProfileComponent, as: 'user')
router.get('/posts/:post_id/comments/:comment_id', to: CommentDetailComponent, as: 'comment')
```

Access parameters in your component via `props`:

```ruby
class UserProfileComponent < Funicular::Component
  def component_mounted
    user_id = props[:id]  # From /users/:id
    User.find(user_id) do |user|
      patch(user: user)
    end
  end

  def render
    div do
      h1 { "User Profile: #{props[:id]}" }
      if state.user
        p { state.user.name }
      end
    end
  end
end
```

## Named Routes and URL Helpers

The `as:` parameter generates URL helper methods:

```ruby
router.get('/users/:id', to: UserProfileComponent, as: 'user')
router.get('/posts/:id/edit', to: EditPostComponent, as: 'edit_post')
router.get('/settings', to: SettingsComponent, as: 'settings')
```

Use helpers via `RouteHelpers` module:

```ruby
include Funicular::RouteHelpers

user_path(123)           # => "/users/123"
edit_post_path(456)      # => "/posts/456/edit"
settings_path            # => "/settings"
```

### With Model Objects

Helpers work with objects that respond to `#id`:

```ruby
user = User.new(id: 123, name: "Alice")
user_path(user)  # => "/users/123"

post = Post.new(id: 456, title: "Hello")
edit_post_path(post)  # => "/posts/456/edit"
```

### In Components

```ruby
class UserCard < Funicular::Component
  include Funicular::RouteHelpers

  def render
    div do
      h3 { props[:user].name }
      link_to user_path(props[:user]), navigate: true do
        span { "View Profile" }
      end
    end
  end
end
```

## Programmatic Navigation

Navigate imperatively using the router instance:

```ruby
class LoginComponent < Funicular::Component
  def handle_login
    User.authenticate(state.credentials) do |user, error|
      if user
        # Navigate to dashboard on success
        Funicular.router.navigate('/dashboard')
      else
        patch(error: error)
      end
    end
  end

  def render
    button(onclick: -> { handle_login }) { "Login" }
  end
end
```

### Getting Current Path

```ruby
Funicular.router.current_path  # => "/users/123"
```

## link_to Helper

The `link_to` helper provides Rails-style navigation and actions. It intelligently chooses between navigation (`<a>` tags) and actions (`<div>` tags) based on context.

### Navigation Links

Use `navigate: true` for client-side page transitions:

```ruby
class Navigation < Funicular::Component
  include Funicular::RouteHelpers

  def render
    nav do
      link_to dashboard_path, navigate: true, class: "nav-link" do
        span { "Dashboard" }
      end

      link_to settings_path, navigate: true, class: "nav-link" do
        span { "Settings" }
      end
    end
  end
end
```

**Behavior**:
- Generates `<a href="/path">` tag
- Normal click: SPA navigation via History API (no page reload)
- Right-click → "Open in New Tab": Opens URL in new tab
- Browser features work: link preview, copy link, etc.

### Action Links

For server actions (create, update, delete), omit `navigate:`:

```ruby
class MessageItem < Funicular::Component
  include Funicular::RouteHelpers

  def render
    div do
      p { props[:message].content }

      # DELETE request to server
      link_to message_path(props[:message]), method: :delete, class: "delete-btn" do
        span { "Delete" }
      end
    end
  end
end
```

**Behavior**:
- Generates `<div>` tag (not `<a>`)
- Sends HTTP request via Fetch API
- Supports all HTTP methods: `:get`, `:post`, `:put`, `:patch`, `:delete`

### Why Different Elements?

| Type | Element | Use Case | Browser Features |
|------|---------|----------|------------------|
| **Navigation** | `<a href>` | Page transitions | ✅ Right-click menu, new tab |
| **Action** | `<div>` | Server operations | ❌ Not a link (semantically correct) |

### Custom Response Handling

Override `handle_link_response` to customize action response handling:

```ruby
class ChatComponent < Funicular::Component
  def handle_link_response(response, path, method)
    if response.error?
      puts "Action failed: #{response.error_message}"
      patch(error: response.error_message)
    else
      puts "Action succeeded!"
      # Update state based on response
      if method == :delete
        # Remove deleted item from state
        patch(messages: state.messages.reject { |m| m.id == response.data[:id] })
      end
    end
  end

  def render
    state.messages.map do |message|
      link_to message_path(message), method: :delete, class: "text-red-600" do
        span { "Delete" }
      end
    end
  end
end
```

### Styling Links

```ruby
class Navigation < Funicular::Component
  include Funicular::RouteHelpers

  styles do
    nav_link base: "px-4 py-2 text-gray-700 hover:bg-gray-100 rounded transition-colors",
             active: "bg-blue-100 text-blue-700 font-semibold"
  end

  def render
    nav(class: "flex gap-2") do
      link_to dashboard_path,
              navigate: true,
              class: s.nav_link(is_current_path?(dashboard_path)) do
        span { "Dashboard" }
      end

      link_to settings_path,
              navigate: true,
              class: s.nav_link(is_current_path?(settings_path)) do
        span { "Settings" }
      end
    end
  end

  def is_current_path?(path)
    Funicular.router.current_path == path
  end
end
```

## Best Practices

### 1. Use Named Routes

```ruby
# ✅ Good: Named routes
router.get('/users/:id', to: UserProfileComponent, as: 'user')
link_to user_path(user), navigate: true

# ❌ Avoid: Hard-coded paths
link_to "/users/#{user.id}", navigate: true
```

### 2. Choose Correct Element Type

```ruby
# ✅ Navigation: use <a> tag
link_to settings_path, navigate: true do
  span { "Settings" }
end

# ✅ Server action: use <div> tag
link_to post_path(post), method: :delete do
  span { "Delete Post" }
end

# ❌ Avoid: Using navigate: true for server actions
link_to post_path(post), method: :delete, navigate: true  # Wrong!
```

### 3. Include RouteHelpers

```ruby
# ✅ Include in components that use routing
class MyComponent < Funicular::Component
  include Funicular::RouteHelpers

  def render
    link_to user_path(user), navigate: true
  end
end
```

### 4. Handle Loading States

```ruby
def handle_navigation
  patch(is_loading: true)

  # Load data, then navigate
  User.find(id) do |user|
    patch(is_loading: false, user: user)
    Funicular.router.navigate(user_path(user))
  end
end
```

### 5. Validate Before Navigation

```ruby
def handle_next_step
  if state.form_valid?
    Funicular.router.navigate('/step-2')
  else
    patch(errors: validate_form)
  end
end
```

## Advanced: Route Guards

Implement route guards by checking conditions in `component_mounted`:

```ruby
class DashboardComponent < Funicular::Component
  def component_mounted
    # Redirect to login if not authenticated
    unless Session.authenticated?
      Funicular.router.navigate('/login')
      return
    end

    # Load dashboard data
    load_suspense_data
  end
end
```

## Integration with Rails Routes

If using Funicular with Rails, you can mirror Rails routes on the client:

```ruby
# Rails routes.rb
Rails.application.routes.draw do
  get '/dashboard', to: 'pages#dashboard'
  resources :users
  resources :posts do
    resources :comments
  end
end

# Funicular router
Funicular.start(container: 'app') do |router|
  router.get('/dashboard', to: DashboardComponent, as: 'dashboard')
  router.get('/users/:id', to: UserProfileComponent, as: 'user')
  router.get('/posts/:post_id/comments/:comment_id', to: CommentDetailComponent, as: 'comment')
end
```

This allows you to use the same path helpers on both client and server.
