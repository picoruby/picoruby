def o1heap_page_size(n)
  optimal_heap_page_size = 2 ** n # This should be power of 2 or just little bit smaller
  ptr_size = 4 # 32-bit
  rvalue_words = 6 # word boxing
  rvalue_size = ptr_size * rvalue_words
  mrb_heap_header_size = ptr_size * 4 # see mruby/src/gc.c
  o1heap_header_size = ptr_size * 4 # see o1heap.c
  (optimal_heap_page_size - mrb_heap_header_size - o1heap_header_size) / rvalue_size
end

MRuby::Gem::Specification.new('picoruby-mruby') do |spec|
  spec.license = ['MIT', 'Apache-2.0', 'BSD-3-Clause', 'BSD']
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby library'

  spec.add_conflict 'picoruby-mrubyc'

  # I don't know why but removing this causes a problem
  # even if build_config has the same define
  spec.cc.defines << "MRB_INT64"

  align = build.cc.defines.find{_1.start_with?("PICORB_ALLOC_ALIGN=")}.then do |define|
    define&.split("=")&.last || 4
  end

  if spec.cc.defines.include?("PICORB_ALLOC_O1HEAP")
    heap_page_size = o1heap_page_size(13)
  else
    heap_page_size = 128
  end
  spec.cc.defines << "MRB_HEAP_PAGE_SIZE=#{heap_page_size}"

  if spec.cc.defines.include?("PICORB_ALLOC_TINYALLOC")
    alloc_dir = "#{dir}/lib/tinyalloc"
    spec.cc.defines << "TA_ALIGNMENT=#{align}"
  elsif spec.cc.defines.include?("PICORB_ALLOC_O1HEAP")
    alloc_dir = "#{dir}/lib/o1heap/o1heap"
    unless File.directory?(alloc_dir)
      FileUtils.cd "#{dir}/lib" do
        sh "git clone https://github.com/pavel-kirienko/o1heap"
      end
    end
  elsif spec.cc.defines.include?("PICORB_ALLOC_TLSF")
    alloc_dir = "#{dir}/lib/tlsf"
    unless File.directory?(alloc_dir)
      FileUtils.cd "#{dir}/lib" do
        sh "git clone https://github.com/mattconte/tlsf"
      end
    end
    if spec.cc.defines.any?{ _1.start_with?("PICORUBY_DEBUG") }
      spec.cc.defines << "_DEBUG"
    end
  elsif spec.cc.defines.include?("PICORB_ALLOC_ESTALLOC")
    spec.cc.defines << "ESTALLOC_ALIGNMENT=#{align}"
    if spec.cc.defines.any?{ _1.start_with?("PICORUBY_DEBUG") }
      spec.cc.defines << "ESTALLOC_DEBUG=1"
    end
    alloc_dir = "#{dir}/lib/estalloc"
  end
  if alloc_dir
    Dir.glob("#{alloc_dir}/*.c").each do |src|
      obj = objfile(src.pathmap("#{build_dir}/allocator/%n"))
      build.libmruby_objs << obj
      file obj => [src, alloc_dir] do |t|
        cc.run t.name, t.prerequisites.first
      end
    end
  end

  dir_for_enhanced_rule = "lib/mruby/src"
  Dir.glob(File.join(dir, "#{dir_for_enhanced_rule}/*.c")).each do |file|
    obj = objfile(file.pathmap("#{build_dir}/#{dir_for_enhanced_rule}/%n"))
    build.libmruby_objs << obj
    file obj => [file] do |t|
      cc.run t.name, t.prerequisites.first
    end
  end
end
