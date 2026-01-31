module USB
  class HID
    include ::USB::HID::Keycode

    # Send a single key press
    def self.send_key(keycode, modifier = 0)
      keyboard_send(modifier, keycode)
    end

    # Send text string (converts to key presses)
    def self.send_text(text)
      text.each_char do |char|
        keycode, modifier = char_to_keycode(char)
        next if keycode.nil? || modifier.nil?

        keyboard_send(modifier, keycode)
        sleep_ms(10)
        keyboard_release
        sleep_ms(10)
      end
    end

    # Move mouse cursor
    def self.move_mouse(x, y, wheel = 0, buttons = 0)
      mouse_move(x, y, wheel, buttons)
    end

    # Click mouse button
    def self.click_mouse(button = MOUSE_BTN_LEFT)
      mouse_move(0, 0, 0, button)
      sleep_ms(50)
      mouse_move(0, 0, 0, 0)
    end

    # Send media control key
    def self.send_media(code)
      media_send(code)
      sleep_ms(50)
      media_send(0)
    end

    private

    # Convert character to keycode and modifier
    def self.char_to_keycode(char)
      case char
      when 'a'..'z'
        [KC_A + (char.ord - 'a'.ord), 0]
      when 'A'..'Z'
        [KC_A + (char.ord - 'A'.ord), KC_LSFT]
      when '1'..'9'
        [KC_1 + (char.ord - '1'.ord), 0]
      when '0'
        [KC_0, 0]
      when ' '
        [KC_SPACE, 0]
      when "\n"
        [KC_ENTER, 0]
      when "\t"
        [KC_TAB, 0]
      when '-'
        [KC_MINUS, 0]
      when '='
        [KC_EQUAL, 0]
      when '['
        [KC_LBRACKET, 0]
      when ']'
        [KC_RBRACKET, 0]
      when '\\'
        [KC_BSLASH, 0]
      when ';'
        [KC_SCOLON, 0]
      when "'"
        [KC_QUOTE, 0]
      when '`'
        [KC_GRAVE, 0]
      when ','
        [KC_COMMA, 0]
      when '.'
        [KC_DOT, 0]
      when '/'
        [KC_SLASH, 0]
      when '!'
        [KC_1, KC_LSFT]
      when '@'
        [KC_2, KC_LSFT]
      when '#'
        [KC_3, KC_LSFT]
      when '$'
        [KC_4, KC_LSFT]
      when '%'
        [KC_5, KC_LSFT]
      when '^'
        [KC_6, KC_LSFT]
      when '&'
        [KC_7, KC_LSFT]
      when '*'
        [KC_8, KC_LSFT]
      when '('
        [KC_9, KC_LSFT]
      when ')'
        [KC_0, KC_LSFT]
      when '_'
        [KC_MINUS, KC_LSFT]
      when '+'
        [KC_EQUAL, KC_LSFT]
      when '{'
        [KC_LBRACKET, KC_LSFT]
      when '}'
        [KC_RBRACKET, KC_LSFT]
      when '|'
        [KC_BSLASH, KC_LSFT]
      when ':'
        [KC_SCOLON, KC_LSFT]
      when '"'
        [KC_QUOTE, KC_LSFT]
      when '~'
        [KC_GRAVE, KC_LSFT]
      when '<'
        [KC_COMMA, KC_LSFT]
      when '>'
        [KC_DOT, KC_LSFT]
      when '?'
        [KC_SLASH, KC_LSFT]
      else
        nil
      end
    end
  end
end
