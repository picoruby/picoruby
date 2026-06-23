class RegexpExtTest < Picotest::Test
  def test_gsub_block_raises_when_receiver_is_modified
    source = "ab" * 1_000

    assert_raise(RuntimeError) do
      source.gsub(/./) do
        source.replace("x" * 100_000)
        "z"
      end
    end
  end
end
