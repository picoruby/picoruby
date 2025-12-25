# PicoRuby Buddie Blog Example

This is an example blog site built with PicoRuby Buddie.
Buddie is a hybrid site generator.
It uses Rake (CRuby) to statically build miscellaneous pages like `about` and article indices, while articles themselves are rendered dynamically in the browser using PicoRuby.wasm.

## Features

- Dynamic article page generation with PicoRuby.wasm
- Static page generation by Rake task
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
├── dist/                        # Site output directory
│   ├── index.html               # Main HTML file
│   ├── 404.html                 # 404 redirect page
│   ├── assets/                  # Static assets
│   │   ├── tailwind-config.js   # Tailwind CSS configuration
│   │   └── styles.css           # Custom styles (customize here!)
│   ├── articles/                # Your markdown articles (write here!), dynamically built in browser
│   │   ├── my_first_post.md
│   │   └── about_picoruby.md
│   ├── misc/                    # Misc pages (About, Contact, etc.), dynamically built in browser
│   │   └── about.md             # Example: /about page
│   ├── generated/               # Statically generated index files
│   │   ├── index_1.json         # Article index for page 1
│   │   └── index_2.json         # Article index for page 2
│   └── about/                   # Generated static page
│       └── index.html           # /about page HTML
├── Rakefile                     # Build tasks
└── README.md
```

## Customizing Design

You can customize the blog design by editing:

- **`tailwind.config.js`**: Tailwind CSS configuration (colors, fonts, breakpoints, etc.)
- **`dist/assets/input.css`**: Custom CSS styles for articles, cards, and layout

After making changes, rebuild the CSS:
```bash
npm run build:css
# or watch for changes
npm run watch:css
```

Example customization in `dist/assets/input.css`:
```css
/* Change article card border color */
.article-card {
  border-left: 4px solid theme('colors.blue.600');
}

/* Change link colors */
.article-content a {
  @apply text-blue-600 hover:text-blue-800;
}
```

Example customization in `tailwind.config.js`:
```javascript
module.exports = {
  theme: {
    extend: {
      colors: {
        primary: '#3b82f6',
      },
    },
  },
}
```

## Getting Started

### Local Development

1. Clone this repository
2. Install dependencies:
   ```bash
   npm install
   ```
3. Add your articles to the `dist/articles/` directory
4. Build the site:
   ```bash
   rake buddie:build
   ```
5. Start the local server:
   ```bash
   rake buddie:server
   ```
6. Open http://localhost:8000 in your browser

For CSS development, you can watch for changes:
```bash
npm run watch:css
```

### Writing Articles

Create markdown files in the `dist/articles/` directory with YAML front matter:

```markdown
---
title: My First Post
date: 2025-12-24
---

This is the content of my first post.
```

## Adding Misc Pages

You can add custom pages (like About, Contact, etc.) without modifying Buddie:

1. Create a markdown file in the `dist/misc/` directory:
   ```bash
   # Create dist/misc/about.md
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
   rake buddie:build
   ```

4. The page will be accessible at `/about` (or whatever you named the file)

**Example:**
- `dist/misc/about.md` → `example.com/about`
- `dist/misc/contact.md` → `example.com/contact`
- `dist/misc/privacy.md` → `example.com/privacy`

The build process automatically generates a dynamic HTML page for each `.md` file in the `dist/misc/` directory.

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
- `/about` - About page (from `dist/misc/about.md`)

## Build Tasks

```bash
# Build CSS with Tailwind
npm run build:css

# Watch CSS for changes during development
npm run watch:css

# Generate article index
rake buddie:generate_index

# Generate pages from dist/misc/*.md
rake buddie:generate_pages

# Build CSS with Tailwind (via rake)
rake buddie:build_css

# Build site for deployment (runs all generation tasks including CSS)
rake buddie:build

# Start local development server
rake buddie:server
```

## Requirements

- Ruby 3.x
- Node.js 18.x or later
- Rake
- JSON gem

For local development:
```bash
gem install rake
npm install
```

For GitHub Actions deployment, dependencies are installed automatically.

## License

MIT
