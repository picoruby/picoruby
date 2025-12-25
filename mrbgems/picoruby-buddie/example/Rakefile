require 'json'
require 'fileutils'
require 'erb'

def load_config
  config_file = File.expand_path('config.yaml', __dir__)
  config = { 'per_page' => 20 }

  if File.exist?(config_file)
    File.read(config_file).each_line do |line|
      next if line.strip.empty? || line.strip.start_with?('#')
      if line =~ /^\s*(\w+):\s*(.+)\s*$/
        key = $1.strip
        value = $2.strip
        config[key] = value.match?(/^\d+$/) ? value.to_i : value
      end
    end
  end

  config
end

namespace :buddie do
  desc "Run server"
  task :server => :build do
    puts "Starting server..."
    FileUtils.cd(File.expand_path('dist', __dir__)) do
      system("ruby -r un -e httpd . -p 8000")
    end
  end

  desc "Generate index.html from template"
  task :generate_html do
    config = load_config
    template_file = File.expand_path('templates/index.html.erb', __dir__)
    output_file = File.expand_path('dist/index.html', __dir__)

    unless File.exist?(template_file)
      puts "Warning: Template file #{template_file} not found."
      next
    end

    template = ERB.new(File.read(template_file))
    html = template.result(binding)
    File.write(output_file, html)
    puts "Generated #{output_file}"
  end

  desc "Generate 404.html from template"
  task :generate_404 do
    template_file = File.expand_path('templates/404.html', __dir__)
    output_file = File.expand_path('dist/404.html', __dir__)

    unless File.exist?(template_file)
      puts "Warning: Template file #{template_file} not found."
      next
    end

    FileUtils.cp(template_file, output_file)
    puts "Generated #{output_file}"
  end

  desc "Generate index.json from articles"
  task :generate_index do
    config = load_config
    per_page = config['per_page'] || 20

    dist_articles_dir = File.expand_path('dist/articles', __dir__)
    generated_dir = File.expand_path('dist/generated', __dir__)

    FileUtils.mkdir_p(generated_dir)

    # Clean up old index files
    Dir.glob(File.join(generated_dir, "index_*.json")).each do |old_file|
      File.delete(old_file)
    end

    articles = []
    Dir.glob(File.join(dist_articles_dir, "*.md")).each do |md_file_path|
      original_content = File.read(md_file_path)

      # Manually parse front matter
      front_matter = {}
      content_without_fm = original_content
      if original_content =~ /\A---\s*\n(?<fm>.*?)\n^---\s*$\n?/m
        # Extract front matter block
        fm_block = Regexp.last_match[:fm]
        fm_block.each_line do |line|
          if line =~ /^\s*(\w+):\s*(.*)\s*$/
            key = $1.strip
            value = $2.strip
            front_matter[key] = value
          end
        end
        # Update content_without_fm by removing front matter
        content_without_fm = original_content.sub(/\A---\s*\n(?<fm>.*?)\n^---\s*$\n?/m, '')
      end

      article_id = File.basename(md_file_path, ".md")

      articles << {
        id: article_id,
        title: front_matter['title'] || 'Untitled',
        date: front_matter['date'] || Time.now.strftime('%Y-%m-%d')
      }
    end

    # Sort articles by date (newest first)
    articles.sort_by! { |a| a[:date] }.reverse!

    # Calculate total pages
    total_articles = articles.length
    total_pages = (total_articles.to_f / per_page).ceil
    total_pages = 1 if total_pages == 0

    puts "Generating #{total_pages} page(s) with #{per_page} articles per page (#{total_articles} total articles)"

    # Generate paginated index files
    (1..total_pages).each do |page_num|
      start_idx = (page_num - 1) * per_page
      end_idx = [start_idx + per_page - 1, total_articles - 1].min
      page_articles = articles[start_idx..end_idx]

      index_data = {
        page: page_num,
        total_pages: total_pages,
        per_page: per_page,
        total_articles: total_articles,
        articles: page_articles
      }

      index_file = File.join(generated_dir, "index_#{page_num}.json")
      File.write(index_file, JSON.pretty_generate(index_data))

      puts "Generated #{index_file} (#{page_articles.length} articles)"
    end
  end

  desc "Generate static pages from misc directory"
  task :generate_pages do
    config = load_config
    dist_dir = File.expand_path('dist', __dir__)
    misc_dist_dir = File.join(dist_dir, 'misc')
    template_file = File.expand_path('templates/index.html.erb', __dir__)

    unless File.exist?(template_file)
      puts "Warning: Template file #{template_file} not found. Skipping page generation."
      next
    end

    FileUtils.mkdir_p(misc_dist_dir)

    template = ERB.new(File.read(template_file))

    Dir.glob(File.join(misc_dist_dir, "*.md")).each do |md_file_path|
      page_name = File.basename(md_file_path, ".md")
      page_dir = File.join(dist_dir, page_name)
      FileUtils.mkdir_p(page_dir)

      # Read markdown file
      content = File.read(md_file_path)

      # Parse front matter
      front_matter = {}
      if content =~ /\A---\s*\n(?<fm>.*?)\n^---\s*$\n?/m
        fm_block = Regexp.last_match[:fm]
        fm_block.each_line do |line|
          if line =~ /^\s*(\w+):\s*(.*)\s*$/
            key = $1.strip
            value = $2.strip
            front_matter[key] = value
          end
        end
        content = content.sub(/\A---\s*\n(?<fm>.*?)\n^---\s*$\n?/m, '')
      end

      title = front_matter['title'] || page_name.gsub('_', ' ').capitalize

      # Create page HTML with embedded Ruby script
      page_html = template.result(binding)
      site_title = config['site_title'] || 'PicoRuby Buddie Blog'
      page_html.sub!(/<title>.*?<\/title>/, "<title>#{title} - #{site_title}</title>")

      # Replace the Ruby script to render the specific page
      page_html.sub!(/<script type="text\/ruby">.*?<\/script>/m, <<~RUBY)
        <script type="text/ruby">
          require 'buddie'
          buddie = Buddie.new
          buddie.render_static_page('misc/#{File.basename(md_file_path)}')
        </script>
      RUBY

      # Write the page HTML
      File.write(File.join(page_dir, 'index.html'), page_html)
      puts "Generated #{page_dir}/index.html"
    end
  end

  desc "Build Prism.js bundle"
  task :build_prism do
    puts "Building Prism.js bundle..."
    system("npm run build:prism")
  end

  desc "Build CSS with Tailwind"
  task :build_css do
    puts "Building CSS with Tailwind..."
    system("npm run build:css")
  end

  desc "Build site for deployment"
  task :build do
    dist_dir = File.expand_path('dist', __dir__)
    FileUtils.mkdir_p(dist_dir)

    puts "Building Buddie blog..."

    # Build Prism.js bundle
    Rake::Task['buddie:build_prism'].invoke

    # Build CSS
    Rake::Task['buddie:build_css'].invoke

    # Generate index.html from template
    Rake::Task['buddie:generate_html'].invoke

    # Generate 404.html from template
    Rake::Task['buddie:generate_404'].invoke

    # Generate article index and copy markdown files
    Rake::Task['buddie:generate_index'].invoke

    # Generate static pages
    Rake::Task['buddie:generate_pages'].invoke

    puts "Build complete! Output in #{dist_dir}"
  end
end
