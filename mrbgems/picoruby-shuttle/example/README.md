# PicoRuby Shuttle Blog Example

This is an example blog site built with PicoRuby Shuttle, a static site generator powered by PicoRuby WASM.

## Features

- Static site generation with PicoRuby WASM
- Query string-based routing (SEO-friendly)
- GitHub Pages compatible
- Markdown article support with front matter
- Automatic article indexing

## Project Structure

```
.
├── .github/
│   └── workflows/
│       └── deploy.yml           # GitHub Actions workflow for deployment
├── articles/                    # Your markdown articles
│   ├── my_first_post.md
│   └── about_picoruby.md
├── misc/                        # Static pages (About, Contact, etc.)
│   └── about.md                 # Example: /about page
├── dist/                        # Generated site (output directory)
│   ├── index.html               # Main HTML file
│   ├── 404.html                 # 404 redirect page
│   ├── assets/                  # Static assets
│   │   ├── tailwind-config.js   # Tailwind CSS configuration
│   │   └── styles.css           # Custom styles (customize here!)
│   ├── articles/                # Generated article files
│   ├── misc/                    # Copied markdown files for SPA
│   └── about/                   # Generated static page
│       └── index.html           # /about page HTML
├── Rakefile                     # Build tasks
└── README.md
```

## Customizing Design

You can customize the blog design by editing files in the `dist/assets/` directory:

- **`assets/tailwind-config.js`**: Tailwind CSS configuration (colors, fonts, etc.)
- **`assets/styles.css`**: Custom CSS styles for articles, cards, and layout

Example customization in `assets/styles.css`:
```css
/* Change article card border color */
.article-card {
  border-left: 4px solid rgb(37, 99, 235); /* blue-600 */
}

/* Change link colors */
.article-content a {
  color: rgb(37, 99, 235); /* blue-600 */
}
```

## Getting Started

### Local Development

1. Clone this repository
2. Add your articles to the `articles/` directory
3. Generate the article index:
   ```bash
   rake shuttle:generate_index
   ```
4. Start the local server:
   ```bash
   rake shuttle:server
   ```
5. Open http://localhost:8000 in your browser

### Writing Articles

Create markdown files in the `articles/` directory with YAML front matter:

```markdown
---
title: My First Post
date: 2025-12-24
---

This is the content of my first post.
```

## Adding Static Pages

You can add custom static pages (like About, Contact, etc.) without modifying Shuttle:

1. Create a markdown file in the `misc/` directory:
   ```bash
   # Create misc/about.md
   ```

2. Add content with front matter:
   ```markdown
   ---
   title: About This Blog
   ---

   # About This Blog

   Your content here...
   ```

3. Build the site:
   ```bash
   rake shuttle:build
   ```

4. The page will be accessible at `/about` (or whatever you named the file)

**Example:**
- `misc/about.md` → `example.com/about`
- `misc/contact.md` → `example.com/contact`
- `misc/privacy.md` → `example.com/privacy`

The build process automatically generates a static HTML page for each `.md` file in the `misc/` directory.

### Deployment to GitHub Pages

This project is configured to automatically deploy to GitHub Pages using GitHub Actions.

1. Push your repository to GitHub
2. Enable GitHub Pages in your repository settings:
   - Go to Settings > Pages
   - Source: GitHub Actions
3. Push to the main/master branch to trigger deployment

The workflow will:
- Generate article index
- Download PicoRuby WASM from npm
- Build and deploy to GitHub Pages

## URL Structure

- `/` - Home page (article list, page 1)
- `/?page=2` - Article list, page 2
- `/?article=my_first_post` - Individual article
- `/about` - Static page (from `misc/about.md`)
- `/contact` - Static page (from `misc/contact.md`)

## Build Tasks

```bash
# Generate article index and copy markdown files
rake shuttle:generate_index

# Generate static pages from misc/*.md
rake shuttle:generate_pages

# Build site for deployment (runs all generation tasks)
rake shuttle:build

# Start local development server
rake shuttle:server
```

## Requirements

- Ruby 3.x
- Rake
- JSON gem

For GitHub Actions deployment, no additional setup is required.

## License

MIT
