class PicoLineFakeBuffer
  def initialize(text)
    @text = text
  end

  def empty?
    @text.empty?
  end

  def dump
    @text
  end
end

class PicoLineFakeEditor
  attr_reader :prompt, :saved, :fed, :refreshed
  attr_accessor :text, :control, :on_start, :idle_handler

  def initialize(text: "", control: 13)
    @text = text
    @control = control
    @saved = 0
    @fed = 0
    @refreshed = 0
  end

  def prompt=(value)
    @prompt = value
  end

  def clear_buffer
  end

  def start
    @on_start&.call
    @idle_handler&.call
    yield self, PicoLineFakeBuffer.new(@text), @control
  end

  def save_history
    @saved += 1
  end

  def feed_at_bottom
    @fed += 1
  end

  def refresh
    @refreshed += 1
  end
end

class PicoLineTest < Picotest::Test
  def test_readline_returns_input_and_saves_history
    editor = PicoLineFakeEditor.new(text: "record")
    line = PicoLine.new(editor: editor)

    assert_equal "record", line.readline("looper")
    assert_equal "looper", editor.prompt
    assert_equal 1, editor.saved
  end

  def test_readline_returns_nil_for_ctrl_d_on_empty_buffer
    editor = PicoLineFakeEditor.new(control: 4)
    line = PicoLine.new(editor: editor)

    assert_nil line.readline("looper")
  end

  def test_say_preserves_active_editor
    editor = PicoLineFakeEditor.new(text: "status")
    line = PicoLine.new(editor: editor)
    editor.on_start = Proc.new { line.say("event") }

    line.readline("looper")

    assert_equal 1, editor.fed
    assert_equal 1, editor.refreshed
  end
end
