class RegexpScanBlockTest < Picotest::Test
  def setup
    require 'regexp'
  end

  def test_scan_block_sees_each_match
    r = []
    "a1b22".scan(/(\d+)/) { |d| r << [d, $~.begin(0)] }
    assert_equal [[["1"], 1], [["22"], 3]], r
  end

  def test_scan_block_empty_matches
    r = []
    "ab".scan(/x*/) { r << $~.begin(0) }
    assert_equal [0, 1, 2], r
  end

  def test_scan_block_without_groups_yields_the_match
    r = []
    "abc".scan(/[ac]/) { |m| r << m }
    assert_equal ["a", "c"], r
  end

  def test_scan_block_returns_the_receiver
    s = "ab"
    assert_equal "ab", s.scan(/a/) {}
  end
end
