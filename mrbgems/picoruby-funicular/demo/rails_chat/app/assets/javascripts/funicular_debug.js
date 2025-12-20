// Funicular Component Highlighter - Development Mode Only
// Highlights components with data-component attributes for easier debugging

(function() {
  'use strict';

  //console.log('[Funicular Debug] Script loaded');

  // Color themes
  const COLORS = {
    green: '#00ff00',      // Fluorescent green (default)
    yellow: '#ffff00',     // Lemon yellow
    pink: '#ff69b4',       // Hot pink
    cyan: '#00ffff'        // Cyan/sky blue
  };

  const DEBUG_CLASS = 'funicular-debug-highlight';

  // Get color dynamically (called each time we need it)
  const getHighlightColor = () => {
    const rubyColor = window.FUNICULAR_DEBUG_COLOR;

    //console.log('[Funicular Debug] Ruby color setting:', rubyColor);

    // If nil/undefined/null, disable the feature
    if (rubyColor === null || rubyColor === undefined || rubyColor === 'nil') {
      //console.log('[Funicular Debug] Debug highlighting is disabled (color is nil)');
      return null;
    }

    return COLORS[rubyColor] || COLORS.green;
  };

  function initComponentHighlighter() {
    const highlightColor = getHighlightColor();

    // Exit early if debug highlighting is disabled
    if (highlightColor === null) {
      //console.log('[Funicular Debug] Component highlighting disabled');
      return;
    }

    //console.log('[Funicular Debug] Initializing component highlighter with color:', highlightColor);

    // Find all elements with data-component attribute
    const components = document.querySelectorAll('[data-component]');
    //console.log(`[Funicular Debug] Found ${components.length} components with data-component`);

    components.forEach(element => {
      highlightElement(element, highlightColor);
    });
  }

  // Use MutationObserver to watch for dynamic DOM changes
  function watchForComponents() {
    //console.log('[Funicular Debug] Starting MutationObserver...');

    const observer = new MutationObserver((mutations) => {
      const highlightColor = getHighlightColor();
      if (highlightColor === null) return;

      mutations.forEach((mutation) => {
        mutation.addedNodes.forEach((node) => {
          if (node.nodeType === 1) { // Element node
            // Check if the added node itself has data-component
            if (node.hasAttribute && node.hasAttribute('data-component')) {
              //console.log('[Funicular Debug] New component detected:', node.getAttribute('data-component'));
              highlightElement(node, highlightColor);
            }
            // Check children
            const children = node.querySelectorAll ? node.querySelectorAll('[data-component]') : [];
            children.forEach(child => {
              //console.log('[Funicular Debug] New child component detected:', child.getAttribute('data-component'));
              highlightElement(child, highlightColor);
            });
          }
        });
      });
    });

    observer.observe(document.body, {
      childList: true,
      subtree: true
    });
  }

  function highlightElement(element, highlightColor) {
    const componentName = element.getAttribute('data-component');

    let tooltipText = componentName;
    if (element.id) {
      tooltipText += ` id=${element.id}`;
    }

    // Check if indicator already exists
    const existingIndicator = element.querySelector('.funicular-debug-indicator');
    if (existingIndicator) {
      // Update tooltip if it's different
      if (existingIndicator.getAttribute('data-tooltip') !== tooltipText) {
        existingIndicator.setAttribute('data-tooltip', tooltipText);
      }
      return;
    }

    // Add debug class
    if (!element.classList.contains(DEBUG_CLASS)) {
      element.classList.add(DEBUG_CLASS);
      // Apply color to the element
      element.style.setProperty('--funicular-highlight-color', highlightColor);
    }

    // Create corner triangle indicator
    const indicator = document.createElement('div');
    indicator.className = 'funicular-debug-indicator';
    indicator.setAttribute('data-tooltip', tooltipText);
    indicator.style.setProperty('--funicular-highlight-color', highlightColor);

    // Position relative wrapper if needed
    const position = window.getComputedStyle(element).position;
    if (position === 'static') {
      element.style.position = 'relative';
    }

    element.appendChild(indicator);

    // Re-add indicator after a short delay if it gets removed
    setTimeout(() => {
      const check = element.querySelector('.funicular-debug-indicator');
      if (!check) {
        const newIndicator = document.createElement('div');
        newIndicator.className = 'funicular-debug-indicator';
        newIndicator.setAttribute('data-tooltip', tooltipText);
        newIndicator.style.setProperty('--funicular-highlight-color', highlightColor);
        element.appendChild(newIndicator);
      }
    }, 100);
  }

  // Initialize on DOM ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
      //console.log('[Funicular Debug] DOMContentLoaded event');
      initComponentHighlighter();
      watchForComponents();
    });
  } else {
    //console.log('[Funicular Debug] DOM already loaded');
    initComponentHighlighter();
    watchForComponents();
  }

  // Reinitialize on turbo:load for Turbo/Hotwire apps
  document.addEventListener('turbo:load', () => {
    //console.log('[Funicular Debug] turbo:load event');
    initComponentHighlighter();
  });

  // Reinitialize on turbolinks:load for older Turbolinks
  document.addEventListener('turbolinks:load', () => {
    //console.log('[Funicular Debug] turbolinks:load event');
    initComponentHighlighter();
  });

  // Expose a manual trigger function for console debugging
  window.funicularDebug = {
    init: initComponentHighlighter,
    checkDOM: () => {
      //console.log('Document readyState:', document.readyState);
      //console.log('Body:', document.body);
      //console.log('App element:', document.getElementById('app'));
      const components = document.querySelectorAll('[data-component]');
      //console.log('Components found:', components.length);
      //components.forEach(c => console.log('  -', c.getAttribute('data-component'), c));
    },
    currentColor: () => {
      //console.log('Current color from Ruby:', window.FUNICULAR_DEBUG_COLOR);
      //console.log('Resolved color:', getHighlightColor());
    }
  };

  const currentColor = getHighlightColor();
  //console.log('[Funicular Debug] Current color:', currentColor ? window.FUNICULAR_DEBUG_COLOR : 'disabled');
})();
