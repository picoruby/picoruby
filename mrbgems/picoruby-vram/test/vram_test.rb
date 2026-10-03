class VRAMTest < Picotest::Test
  def test_pages_release_memory
    skip 'mruby/c reference counting regression' unless femtoruby?
    vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
    extract_pages(vram, false)
    before = free_memory
    extract_pages(vram, false)
    assert_equal before, free_memory
  end

  def test_dirty_pages_release_memory
    skip 'mruby/c reference counting regression' unless femtoruby?
    vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
    extract_pages(vram, true)
    before = free_memory
    extract_pages(vram, true)
    assert_equal before, free_memory
  end

  def test_page_buffers_survive_releasing_results
    vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
    extract_pages(vram, false)
    extract_pages(vram, true)
    vram.fill(0)
    vram.set_pixel(0, 0, 1)
    pages = vram.pages
    assert_equal 8, pages.length
    assert_equal [0, 0, "\x01" + "\x00" * 127], pages[0]
    assert_equal [0, 7, "\x00" * 128], pages[7]
  end

  def test_page_buffers_survive_releasing_vram
    vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
    vram.fill(1)
    pages = vram.pages
    vram = nil
    GC.start
    assert_equal "\xFF" * 128, pages[0][2]
    assert_equal "\xFF" * 128, pages[7][2]
  end

  def test_dirty_flags
    vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
    vram.pages
    assert_equal [], vram.dirty_pages
    vram.set_pixel(0, 8, 1)
    assert_equal [[0, 1, "\x01" + "\x00" * 127]], vram.dirty_pages(false)
    vram.pages(false)
    assert_equal 1, vram.dirty_pages.length
    assert_equal [], vram.dirty_pages
    vram.fill(1)
    assert_equal 8, vram.dirty_pages.length
    assert_equal [], vram.dirty_pages
  end

  def extract_pages(vram, dirty)
    100.times do
      if dirty
        vram.fill(1)
        vram.dirty_pages
      else
        vram.pages
      end
    end
    nil
  end

  def free_memory
    require 'picorubyvm'
    GC.start
    PicoRubyVM.memory_statistics[:free]
  end
end
