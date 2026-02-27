class BufferTest < Picotest::Test
  def setup
    @buf = Editor::Buffer.new
  end

  # --- Initialization ---

  def test_initial_state
    assert_equal 0, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
    assert_equal [""], @buf.lines
  end

  # --- Cursor movement ---

  def test_right_moves_cursor
    @buf.lines = ["hello"]
    @buf.put :RIGHT
    assert_equal 1, @buf.cursor_x
  end

  def test_right_wraps_to_next_line
    @buf.lines = ["ab", "cd"]
    @buf.put :RIGHT
    @buf.put :RIGHT
    @buf.put :RIGHT
    assert_equal 0, @buf.cursor_x
    assert_equal 1, @buf.cursor_y
  end

  def test_right_stops_at_end_of_last_line
    @buf.lines = ["ab"]
    @buf.put :RIGHT
    @buf.put :RIGHT
    @buf.put :RIGHT
    assert_equal 2, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
  end

  def test_left_moves_cursor
    @buf.lines = ["hello"]
    @buf.put :RIGHT
    @buf.put :RIGHT
    @buf.put :LEFT
    assert_equal 1, @buf.cursor_x
  end

  def test_left_wraps_to_previous_line
    @buf.lines = ["ab", "cd"]
    @buf.move_to(0, 1)
    @buf.put :LEFT
    assert_equal 2, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
  end

  def test_left_stops_at_beginning
    @buf.lines = ["ab"]
    @buf.put :LEFT
    assert_equal 0, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
  end

  def test_down_moves_cursor
    @buf.lines = ["aa", "bb"]
    @buf.put :DOWN
    assert_equal 1, @buf.cursor_y
  end

  def test_down_stops_at_last_line
    @buf.lines = ["aa"]
    @buf.put :DOWN
    assert_equal 0, @buf.cursor_y
  end

  def test_up_moves_cursor
    @buf.lines = ["aa", "bb"]
    @buf.move_to(0, 1)
    @buf.put :UP
    assert_equal 0, @buf.cursor_y
  end

  def test_up_stops_at_first_line
    @buf.lines = ["aa"]
    @buf.put :UP
    assert_equal 0, @buf.cursor_y
  end

  # --- Head / Tail / Home / Bottom ---

  def test_head
    @buf.lines = ["hello"]
    @buf.move_to(3, 0)
    @buf.head
    assert_equal 0, @buf.cursor_x
  end

  def test_tail
    @buf.lines = ["hello"]
    @buf.tail
    assert_equal 5, @buf.cursor_x
  end

  def test_home
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(1, 2)
    @buf.home
    assert_equal 0, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
  end

  def test_bottom
    @buf.lines = ["aa", "bb", "cc"]
    @buf.bottom
    assert_equal 2, @buf.cursor_y
  end

  # --- Text insertion ---

  def test_put_char
    @buf.put "a"
    assert_equal ["a"], @buf.lines
    assert_equal 1, @buf.cursor_x
  end

  def test_put_multiple_chars
    @buf.put "h"
    @buf.put "i"
    assert_equal ["hi"], @buf.lines
    assert_equal 2, @buf.cursor_x
  end

  def test_put_char_in_middle
    @buf.lines = ["hllo"]
    @buf.move_to(1, 0)
    @buf.put "e"
    assert_equal ["hello"], @buf.lines
    assert_equal 2, @buf.cursor_x
  end

  def test_put_enter
    @buf.lines = ["hello"]
    @buf.move_to(3, 0)
    @buf.put :ENTER
    assert_equal ["hel", "lo"], @buf.lines
    assert_equal 0, @buf.cursor_x
    assert_equal 1, @buf.cursor_y
  end

  def test_put_enter_at_end
    @buf.lines = ["hello"]
    @buf.move_to(5, 0)
    @buf.put :ENTER
    assert_equal ["hello", ""], @buf.lines
  end

  def test_put_tab
    @buf.put :TAB
    assert_equal ["  "], @buf.lines
    assert_equal 2, @buf.cursor_x
  end

  # --- Backspace ---

  def test_bspace_in_middle
    @buf.lines = ["hello"]
    @buf.move_to(3, 0)
    @buf.put :BSPACE
    assert_equal ["helo"], @buf.lines
    assert_equal 2, @buf.cursor_x
  end

  def test_bspace_at_end
    @buf.lines = ["hello"]
    @buf.move_to(5, 0)
    @buf.put :BSPACE
    assert_equal ["hell"], @buf.lines
  end

  def test_bspace_merges_lines
    @buf.lines = ["hello", "world"]
    @buf.move_to(0, 1)
    @buf.put :BSPACE
    assert_equal ["helloworld"], @buf.lines
    assert_equal 5, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
  end

  def test_bspace_at_beginning_of_first_line
    @buf.lines = ["hello"]
    @buf.put :BSPACE
    assert_equal ["hello"], @buf.lines
  end

  # --- Delete ---

  def test_delete
    @buf.lines = ["hello"]
    @buf.move_to(1, 0)
    @buf.delete
    assert_equal ["hllo"], @buf.lines
  end

  def test_delete_line
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(0, 1)
    result = @buf.delete_line
    assert_equal "bb", result
    assert_equal ["aa", "cc"], @buf.lines
  end

  # --- Insert line ---

  def test_insert_line
    @buf.lines = ["aa", "cc"]
    @buf.move_to(0, 1)
    @buf.insert_line("bb")
    assert_equal ["aa", "bb", "cc"], @buf.lines
  end

  # --- Replace char ---

  def test_replace_char
    @buf.lines = ["hello"]
    @buf.move_to(1, 0)
    @buf.replace_char("a")
    assert_equal ["hallo"], @buf.lines
  end

  def test_replace_char_beyond_length
    @buf.lines = ["hi"]
    @buf.move_to(5, 0)
    @buf.replace_char("a")
    assert_equal ["hi"], @buf.lines
  end

  # --- Word navigation ---

  def test_word_forward
    @buf.lines = ["hello world"]
    @buf.word_forward
    assert_equal 6, @buf.cursor_x
  end

  def test_word_forward_at_end_of_line
    @buf.lines = ["hello", "world"]
    @buf.move_to(5, 0)
    @buf.word_forward
    assert_equal 0, @buf.cursor_x
    assert_equal 1, @buf.cursor_y
  end

  def test_word_backward
    @buf.lines = ["hello world"]
    @buf.move_to(8, 0)
    @buf.word_backward
    assert_equal 6, @buf.cursor_x
  end

  def test_word_backward_at_beginning
    @buf.lines = ["hello", "world"]
    @buf.move_to(0, 1)
    @buf.word_backward
    assert_equal 0, @buf.cursor_y
  end

  def test_word_end
    @buf.lines = ["hello world"]
    @buf.word_end
    assert_equal 4, @buf.cursor_x
  end

  def test_word_end_skips_spaces
    @buf.lines = ["hello world"]
    @buf.move_to(4, 0)
    @buf.word_end
    assert_equal 10, @buf.cursor_x
  end

  # --- word_char? ---

  def test_word_char
    assert_true @buf.word_char?("a")
    assert_true @buf.word_char?("Z")
    assert_true @buf.word_char?("0")
    assert_true @buf.word_char?("_")
    assert_false @buf.word_char?(" ")
    assert_false @buf.word_char?(".")
    assert_false @buf.word_char?(nil)
  end

  # --- move_to ---

  def test_move_to
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(1, 2)
    assert_equal 1, @buf.cursor_x
    assert_equal 2, @buf.cursor_y
  end

  # --- Selection ---

  def test_start_and_clear_selection
    @buf.lines = ["hello"]
    @buf.move_to(2, 0)
    @buf.start_selection(:char)
    assert_true @buf.has_selection?
    assert_equal :char, @buf.selection_mode
    assert_equal 2, @buf.selection_start_x
    assert_equal 0, @buf.selection_start_y
    @buf.clear_selection
    assert_false @buf.has_selection?
    assert_nil @buf.selection_mode
  end

  def test_selection_range_same_line
    @buf.lines = ["hello world"]
    @buf.move_to(2, 0)
    @buf.start_selection(:char)
    @buf.move_to(7, 0)
    range = @buf.selection_range
    assert_equal [0, 2, 0, 7], range
  end

  def test_selection_range_reversed
    @buf.lines = ["hello world"]
    @buf.move_to(7, 0)
    @buf.start_selection(:char)
    @buf.move_to(2, 0)
    range = @buf.selection_range
    assert_equal [0, 2, 0, 7], range
  end

  def test_selection_range_multi_line
    @buf.lines = ["hello", "world", "test"]
    @buf.move_to(2, 0)
    @buf.start_selection(:char)
    @buf.move_to(3, 2)
    range = @buf.selection_range
    assert_equal [0, 2, 2, 3], range
  end

  def test_selection_range_line_mode
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(0, 0)
    @buf.start_selection(:line)
    @buf.move_to(0, 2)
    range = @buf.selection_range
    assert_equal [0, 0, 2, 0], range
  end

  # --- selected_text ---

  def test_selected_text_single_line
    @buf.lines = ["hello world"]
    @buf.move_to(0, 0)
    @buf.start_selection(:char)
    @buf.move_to(4, 0)
    assert_equal "hello", @buf.selected_text
  end

  def test_selected_text_multi_line
    @buf.lines = ["hello", "beautiful", "world"]
    @buf.move_to(2, 0)
    @buf.start_selection(:char)
    @buf.move_to(3, 2)
    assert_equal "llo\nbeautiful\nworl", @buf.selected_text
  end

  def test_selected_text_line_mode
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(0, 0)
    @buf.start_selection(:line)
    @buf.move_to(0, 1)
    assert_equal "aa\nbb", @buf.selected_text
  end

  def test_selected_text_line_mode_single_line
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(0, 1)
    @buf.start_selection(:line)
    assert_equal "bb", @buf.selected_text
  end

  # --- delete_selected_text ---

  def test_delete_selected_text_single_line
    @buf.lines = ["hello world"]
    @buf.move_to(0, 0)
    @buf.start_selection(:char)
    @buf.move_to(4, 0)
    deleted = @buf.delete_selected_text
    assert_equal "hello", deleted
    assert_equal [" world"], @buf.lines
    assert_equal 0, @buf.cursor_x
  end

  def test_delete_selected_text_multi_line
    @buf.lines = ["hello", "beautiful", "world"]
    @buf.move_to(2, 0)
    @buf.start_selection(:char)
    @buf.move_to(3, 2)
    deleted = @buf.delete_selected_text
    assert_equal "llo\nbeautiful\nworl", deleted
    assert_equal ["hed"], @buf.lines
    assert_equal 2, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
  end

  def test_delete_selected_text_line_mode
    @buf.lines = ["aa", "bb", "cc"]
    @buf.move_to(0, 1)
    @buf.start_selection(:line)
    @buf.move_to(0, 2)
    deleted = @buf.delete_selected_text
    assert_equal "bb\ncc", deleted
    assert_equal ["aa"], @buf.lines
    assert_equal 0, @buf.cursor_y
  end

  def test_delete_selected_text_line_mode_all_lines
    @buf.lines = ["aa", "bb"]
    @buf.move_to(0, 0)
    @buf.start_selection(:line)
    @buf.move_to(0, 1)
    @buf.delete_selected_text
    assert_equal [""], @buf.lines
    assert_equal 0, @buf.cursor_y
  end

  # --- insert_lines_below ---

  def test_insert_lines_below
    @buf.lines = ["aa", "dd"]
    @buf.move_to(0, 0)
    @buf.insert_lines_below(["bb", "cc"])
    assert_equal ["aa", "bb", "cc", "dd"], @buf.lines
    assert_equal 1, @buf.cursor_y
    assert_equal 0, @buf.cursor_x
  end

  def test_insert_lines_below_at_end
    @buf.lines = ["aa"]
    @buf.move_to(0, 0)
    @buf.insert_lines_below(["bb"])
    assert_equal ["aa", "bb"], @buf.lines
    assert_equal 1, @buf.cursor_y
  end

  # --- insert_string_after_cursor ---

  def test_insert_string_after_cursor_single
    @buf.lines = ["hllo"]
    @buf.move_to(0, 0)
    @buf.insert_string_after_cursor("e")
    assert_equal ["hello"], @buf.lines
    assert_equal 1, @buf.cursor_x
  end

  def test_insert_string_after_cursor_at_end
    @buf.lines = ["hi"]
    @buf.move_to(1, 0)
    @buf.insert_string_after_cursor("!")
    assert_equal ["hi!"], @buf.lines
    assert_equal 2, @buf.cursor_x
  end

  def test_insert_string_after_cursor_multi_line
    @buf.lines = ["hello world"]
    @buf.move_to(4, 0)
    @buf.insert_string_after_cursor("ABC\nDEF")
    assert_equal ["helloABC", "DEF world"], @buf.lines
    assert_equal 2, @buf.cursor_x
    assert_equal 1, @buf.cursor_y
  end

  def test_insert_string_after_cursor_on_empty_line
    @buf.lines = [""]
    @buf.move_to(0, 0)
    @buf.insert_string_after_cursor("abc")
    assert_equal ["abc"], @buf.lines
    assert_equal 2, @buf.cursor_x
  end

  # --- dump ---

  def test_dump
    @buf.lines = ["hello", "world"]
    assert_equal "hello\nworld", @buf.dump
  end

  # --- changed flag ---

  def test_changed_flag
    assert_false @buf.changed
    @buf.put "a"
    assert_true @buf.changed
  end

  # --- current_line / current_char ---

  def test_current_line
    @buf.lines = ["aa", "bb"]
    @buf.move_to(0, 1)
    assert_equal "bb", @buf.current_line
  end

  def test_current_char
    @buf.lines = ["hello"]
    @buf.move_to(1, 0)
    assert_equal "e", @buf.current_char
  end

  # --- clear ---

  def test_clear_resets_state
    @buf.lines = ["hello", "world"]
    @buf.move_to(3, 1)
    @buf.start_selection(:char)
    @buf.clear
    assert_equal [""], @buf.lines
    assert_equal 0, @buf.cursor_x
    assert_equal 0, @buf.cursor_y
    assert_false @buf.has_selection?
  end

  # --- dirty tracking ---

  def test_dirty_on_cursor_move
    @buf.clear_dirty
    @buf.lines = ["hello"]
    @buf.put :RIGHT
    assert_equal :cursor, @buf.dirty
  end

  def test_dirty_on_content_change
    @buf.clear_dirty
    @buf.put "a"
    assert_equal :content, @buf.dirty
  end

  def test_dirty_on_structure_change
    @buf.clear_dirty
    @buf.put :ENTER
    assert_equal :structure, @buf.dirty
  end

  def test_dirty_never_downgrades
    @buf.clear_dirty
    @buf.put :ENTER
    @buf.put :RIGHT
    assert_equal :structure, @buf.dirty
  end
end
