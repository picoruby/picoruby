# Rails-style Form Builder

Funicular provides a powerful `form_for` helper that brings Rails-style form building to your Pure Ruby frontend. The FormBuilder automatically manages form state, handles two-way data binding, and displays validation errors.

## Table of Contents

- [Basic Usage](#basic-usage)
- [Available Field Types](#available-field-types)
- [Automatic Error Display](#automatic-error-display)
- [Nested Fields](#nested-fields)
- [Customizing Error Styles](#customizing-error-styles)
- [Form Submission](#form-submission)
- [Boolean Attributes](#boolean-attributes)
- [Examples](#examples)

## Basic Usage

```ruby
class SignupComponent < Funicular::Component
  def initialize_state
    {
      user: { username: "", email: "", password: "" },
      errors: {}
    }
  end

  def handle_submit(form_data)
    # Create user via O-R-M or API call
    User.create(form_data) do |user, errors|
      if errors
        patch(errors: errors)
      else
        # Success: redirect or show success message
        puts "User created: #{user.username}"
      end
    end
  end

  def render
    form_for(:user, on_submit: :handle_submit) do |f|
      f.label(:username)
      f.text_field(:username, class: "form-input", autofocus: true)

      f.label(:email)
      f.email_field(:email, class: "form-input")

      f.label(:password)
      f.password_field(:password, class: "form-input")

      f.submit("Sign Up", class: "btn btn-primary")
    end
  end
end
```

## Available Field Types

The FormBuilder supports all standard HTML input types:

### Text Inputs

```ruby
f.text_field(:username, class: "w-full p-2 border rounded")
f.password_field(:password, class: "w-full p-2 border rounded")
f.email_field(:email, class: "w-full p-2 border rounded")
f.number_field(:age, min: 0, max: 120)
```

### Textarea

```ruby
f.textarea(:bio, rows: 5, class: "w-full p-2 border rounded")
```

### Checkbox

```ruby
f.checkbox(:agree_to_terms, class: "mr-2")
f.label(:agree_to_terms) { "I agree to the terms and conditions" }
```

### Select Dropdown

```ruby
# Simple array of options
f.select(:country, ["USA", "Canada", "UK", "Japan"], class: "w-full p-2 border rounded")

# Array of [label, value] pairs
f.select(:role, [["Administrator", "admin"], ["Editor", "editor"], ["Viewer", "viewer"]])

# With prompt
f.select(:category, ["Tech", "Science", "Art"], prompt: "Select a category")
```

### File Upload

```ruby
f.file_field(:avatar, accept: "image/*", class: "w-full")
```

### Label

```ruby
f.label(:username)  # Auto-generates "Username" from field name
f.label(:email) { "Email Address" }  # Custom label text
```

### Submit Button

```ruby
f.submit("Sign Up", class: "btn btn-primary")
f.submit("Save Changes", disabled: state.is_saving)
```

## Automatic Error Display

The FormBuilder automatically displays validation errors when they exist in `state.errors`. Errors are displayed below their respective fields with customizable styling.

```ruby
class LoginComponent < Funicular::Component
  def initialize_state
    {
      user: { email: "", password: "" },
      errors: {}
    }
  end

  def handle_submit(form_data)
    User.authenticate(form_data) do |user, errors|
      if errors
        # Errors format: { email: "Email is invalid", password: "Password is required" }
        patch(errors: errors)
      else
        # Success: navigate to dashboard
      end
    end
  end

  def render
    form_for(:user, on_submit: :handle_submit) do |f|
      f.email_field(:email, class: "w-full p-2 border rounded")
      # If errors[:email] exists, it's displayed in red below the field
      # Field automatically gets border-red-500 class

      f.password_field(:password, class: "w-full p-2 border rounded")
      # If errors[:password] exists, it's displayed in red below the field

      f.submit("Login")
    end
  end
end
```

### Error Behavior

When `state.errors` contains a key matching a field name:
1. The error message is displayed below the field
2. The field gets an error class added (default: `border-red-500`)
3. Error messages are styled with `error_class` (default: `text-red-600 text-sm mt-1`)

## Nested Fields

The FormBuilder supports nested fields using dot notation:

```ruby
class UserProfileComponent < Funicular::Component
  def initialize_state
    {
      user: {
        name: "",
        profile: { bio: "", location: "", website: "" },
        settings: { theme: "light", notifications: true }
      },
      errors: {}
    }
  end

  def render
    form_for(:user, on_submit: :handle_submit) do |f|
      f.text_field(:name)

      # Nested profile fields
      f.text_field("profile.bio")
      f.text_field("profile.location")
      f.text_field("profile.website")

      # Nested settings fields
      f.select("settings.theme", ["light", "dark"])
      f.checkbox("settings.notifications")

      f.submit("Save")
    end
  end
end
```

Nested errors also work:

```ruby
def handle_submit(form_data)
  User.update(form_data) do |user, errors|
    # Errors format: { "profile.bio": "Bio is too long", name: "Name is required" }
    patch(errors: errors)
  end
end
```

## Customizing Error Styles

### Global Configuration

Configure error display styles globally:

```ruby
# In your initializer (e.g., app/funicular/initializer.rb)
Funicular.configure_forms do |config|
  config[:error_class] = "text-red-600 text-sm mt-1"
  config[:field_error_class] = "border-red-500 border-2"
end
```

### Per-Form Configuration

```ruby
form_for(:user,
         on_submit: :handle_submit,
         error_class: "error-text",
         field_error_class: "error-border") do |f|
  f.email_field(:email)
  f.submit("Submit")
end
```

## Form Submission

### Submit Handler

The `on_submit` callback receives the form data as a hash:

```ruby
def handle_submit(form_data)
  # form_data is the current state of the form model
  # For form_for(:user), form_data = state.user

  puts form_data  # { username: "alice", email: "alice@example.com", password: "..." }

  # Make API call
  User.create(form_data) do |user, errors|
    if errors
      patch(errors: errors)
    else
      # Navigate to success page
      Funicular.router.navigate('/dashboard')
    end
  end
end
```

### Preventing Default Submit

The FormBuilder automatically prevents the default form submission (which would reload the page). You don't need to call `event.preventDefault()`.

### Submit Button State

Disable the submit button while processing:

```ruby
def initialize_state
  { user: { email: "" }, is_submitting: false }
end

def handle_submit(form_data)
  patch(is_submitting: true)

  User.create(form_data) do |user, errors|
    patch(is_submitting: false, errors: errors || {})
  end
end

def render
  form_for(:user, on_submit: :handle_submit) do |f|
    f.email_field(:email)
    f.submit(
      state.is_submitting ? "Submitting..." : "Submit",
      disabled: state.is_submitting
    )
  end
end
```

## Boolean Attributes

All HTML boolean attributes are supported and can be set to `true` or `false`:

```ruby
f.text_field(:username,
  autofocus: true,
  required: true,
  readonly: state.is_locked,
  disabled: state.is_processing
)

f.checkbox(:subscribe,
  checked: state.user[:subscribe],
  required: true
)

f.textarea(:comment,
  autofocus: true,
  required: state.is_required
)

f.submit("Send",
  disabled: state.message.empty?
)
```

Supported boolean attributes:
- `autofocus`
- `disabled`
- `checked`
- `readonly`
- `required`
- `multiple`

## Examples

### Complete Registration Form

```ruby
class RegistrationComponent < Funicular::Component
  def initialize_state
    {
      user: {
        username: "",
        email: "",
        password: "",
        password_confirmation: "",
        country: "",
        agree_to_terms: false
      },
      errors: {},
      is_submitting: false
    }
  end

  def handle_submit(form_data)
    patch(is_submitting: true, errors: {})

    User.create(form_data) do |user, errors|
      if errors
        patch(is_submitting: false, errors: errors)
      else
        patch(is_submitting: false)
        # Navigate to success page
        Funicular.router.navigate('/welcome')
      end
    end
  end

  def render
    div(class: "max-w-md mx-auto p-6") do
      h1(class: "text-2xl font-bold mb-6") { "Create Account" }

      form_for(:user, on_submit: :handle_submit, class: "space-y-4") do |f|
        div do
          f.label(:username) { "Username" }
          f.text_field(:username, class: "w-full p-2 border rounded", autofocus: true)
        end

        div do
          f.label(:email) { "Email" }
          f.email_field(:email, class: "w-full p-2 border rounded")
        end

        div do
          f.label(:password) { "Password" }
          f.password_field(:password, class: "w-full p-2 border rounded")
        end

        div do
          f.label(:password_confirmation) { "Confirm Password" }
          f.password_field(:password_confirmation, class: "w-full p-2 border rounded")
        end

        div do
          f.label(:country) { "Country" }
          f.select(:country, ["USA", "Canada", "UK", "Japan"], prompt: "Select country", class: "w-full p-2 border rounded")
        end

        div(class: "flex items-center") do
          f.checkbox(:agree_to_terms, class: "mr-2")
          f.label(:agree_to_terms) { "I agree to the Terms and Conditions" }
        end

        f.submit(
          state.is_submitting ? "Creating Account..." : "Create Account",
          disabled: state.is_submitting || !state.user[:agree_to_terms],
          class: "w-full bg-blue-600 text-white py-2 rounded hover:bg-blue-700 disabled:bg-gray-400"
        )
      end
    end
  end
end
```

### Form with File Upload

```ruby
class ProfilePictureComponent < Funicular::Component
  def initialize_state
    { avatar_url: "", is_uploading: false }
  end

  def handle_file_change(event)
    file = event.target[:files][0]
    return unless file

    patch(is_uploading: true)

    # Use FileUpload helper (see JS Integration docs)
    Funicular::FileUpload.upload(file) do |result|
      if result[:error]
        puts "Upload failed: #{result[:error]}"
        patch(is_uploading: false)
      else
        patch(avatar_url: result[:url], is_uploading: false)
      end
    end
  end

  def render
    div do
      form_for(:profile) do |f|
        f.file_field(:avatar, accept: "image/*", onchange: :handle_file_change)

        if state.is_uploading
          p { "Uploading..." }
        elsif !state.avatar_url.empty?
          img(src: state.avatar_url, class: "w-32 h-32 rounded-full")
        end
      end
    end
  end
end
```
