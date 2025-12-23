// PicoRuby DevTools panel script

class PicoRubyDebugger {
  constructor() {
    this.status = document.getElementById('status');
    this.replOutput = document.getElementById('replOutput');
    this.replInput = document.getElementById('replInput');

    // Component debug elements
    this.componentsSection = document.getElementById('componentsSection');
    this.inspectorSection = document.getElementById('inspectorSection');
    this.componentTree = document.getElementById('componentTree');
    this.componentInspector = document.getElementById('componentInspector');

    this.selectedComponentId = null;
    this.expandedComponents = new Set();
    this.refreshDebounceTimer = null;
    this.autoRefreshInterval = null;
    this.lastComponentTreeHash = null;
    this.setupEventListeners();
    this.checkConnection();
  }

  setupEventListeners() {
    this.replInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        this.evalCode(this.replInput.value);
        this.replInput.value = '';
      }
    });
  }

  checkConnection() {
    // Confirm the existence of picorubyModule
    this.evalInPage('typeof window.picorubyModule')
      .then(result => {
        if (result === 'undefined') {
          this.updateStatus('PicoRuby Module not found');
          return;
        }

        // Check available functions
        return this.evalInPage(`
          (function() {
            if (typeof window.picorubyModule === 'undefined') return { error: 'picorubyModule undefined' };
            const Module = window.picorubyModule;
            return {
              hasModule: true,
              hasPicorbInit: typeof Module._picorb_init !== 'undefined',
              hasMrbRunStep: typeof Module._mrb_run_step !== 'undefined',
              hasMrbTickWasm: typeof Module._mrb_tick_wasm !== 'undefined',
              hasCreateTask: typeof Module._picorb_create_task !== 'undefined',
              moduleKeys: Object.keys(Module).filter(k => k.startsWith('_')).slice(0, 20)
            };
          })()
        `);
      })
      .then(info => {
        if (!info) return;

        if (info.hasModule) {
          this.updateStatus('Connected to PicoRuby ✓');
          this.displayConnectionInfo(info);
          // Check for component debug mode
          this.checkComponentDebugMode();
        } else {
          this.updateStatus('PicoRuby not detected');
        }
      })
      .catch(err => {
        console.error('Connection check error:', err);
        this.updateStatus('Error: ' + err.message);
      });
  }

  displayConnectionInfo(info) {
    // Display connection info in REPL
    this.replOutput.innerHTML = `
      <div style="color: #4ec9b0; padding: 8px;">
        <strong>PicoRuby WASM Debugger</strong><br><br>
        <br>
        <span style="color: #888;">Type Ruby code below to test REPL (WIP)</span>
      </div>
    `;
  }

  updateStatus(message) {
    this.status.textContent = message;
  }

  evalCode(code) {
    if (!code.trim()) return;

    const entry = document.createElement('div');
    entry.className = 'repl-entry';
    entry.innerHTML = `<div class="repl-input-line">&gt;&gt; ${this.escapeHtml(code)}</div>`;

    if (this.replOutput.querySelector('.empty-state')) {
      this.replOutput.innerHTML = '';
    }

    this.replOutput.appendChild(entry);

    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined') {
          return { error: 'PicoRuby not available' };
        }
        try {
          const jsonStr = window.picorubyModule.ccall('mrb_eval_string', 'string', ['string'], [${JSON.stringify(code)}]);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(response => {
      if (response.error) {
        const errorLine = document.createElement('div');
        errorLine.className = 'repl-error';
        errorLine.textContent = response.error;
        entry.appendChild(errorLine);
      } else {
        const outputLine = document.createElement('div');
        outputLine.className = 'repl-output-line';
        outputLine.textContent = response.result;
        entry.appendChild(outputLine);
      }
      this.replOutput.scrollTop = this.replOutput.scrollHeight;
    }).catch(err => {
      console.error('REPL eval error:', err);
      const errorLine = document.createElement('div');
      errorLine.className = 'repl-error';
      errorLine.textContent = 'DevTools error: ' + err.message;
      entry.appendChild(errorLine);
      this.replOutput.scrollTop = this.replOutput.scrollHeight;
    });
  }


  evalInPage(code) {
    return new Promise((resolve, reject) => {
      try {
        chrome.devtools.inspectedWindow.eval(code, (result, exceptionInfo) => {
          if (exceptionInfo) {
            console.error('Eval error:', exceptionInfo);
            reject(new Error(exceptionInfo.description || exceptionInfo.value || 'Eval failed'));
          } else {
            resolve(result);
          }
        });
      } catch (err) {
        console.error('Chrome API error:', err);
        reject(err);
      }
    });
  }

  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  // Component Debug Mode
  checkComponentDebugMode() {
    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined') {
          return { available: false, reason: 'PicoRuby not loaded' };
        }
        const Module = window.picorubyModule;
        const code = "global_variables.include?(:$__funicular_debug__) ? 'enabled' : 'disabled'";
        const jsonStr = Module.ccall('mrb_eval_string', 'string', ['string'], [code]);
        const result = JSON.parse(jsonStr);
        return {
          available: result.result === '"enabled"',
          reason: result.result === '"enabled"' ? null : 'Not in development mode'
        };
      })()
    `).then(result => {
      if (result.available) {
        this.enableComponentDebug();
      } else {
        this.showComponentDebugUnavailable(result.reason);
      }
    }).catch(err => {
      console.error('Component debug check error:', err);
      this.showComponentDebugUnavailable('Error checking debug mode');
    });
  }

  enableComponentDebug() {
    if (!this.componentsSection || !this.inspectorSection) return;
    this.componentsSection.style.display = 'flex';
    this.inspectorSection.style.display = 'flex';
    this.updateStatus('Component Debug enabled ✓');
    setTimeout(() => {
      this.refreshComponents();
      this.startAutoRefresh();
    }, 100);
  }

  showComponentDebugUnavailable(reason) {
    if (!this.componentsSection) return;
    this.componentsSection.style.display = 'none';
    this.stopAutoRefresh();
  }

  startAutoRefresh() {
    // Stop existing interval if any
    this.stopAutoRefresh();

    // Poll every 500ms to check for component changes
    this.autoRefreshInterval = setInterval(() => {
      this.checkAndRefreshComponents();
    }, 500);
  }

  stopAutoRefresh() {
    if (this.autoRefreshInterval) {
      clearInterval(this.autoRefreshInterval);
      this.autoRefreshInterval = null;
    }
  }

  checkAndRefreshComponents() {
    // Get current component tree hash
    this.evalInPage(`
      (function() {
        const Module = window.picorubyModule;
        const jsonStr = Module.ccall('mrb_get_component_debug_info', 'string', ['string'], ['component_tree']);
        return jsonStr;
      })()
    `).then(response => {
      if (!response) return;

      // Create a simple hash of the component tree
      const currentHash = this.simpleHash(response);

      // If hash changed, refresh the display
      if (this.lastComponentTreeHash !== null && this.lastComponentTreeHash !== currentHash) {
        console.log('Component tree changed, refreshing...');
        this.getComponentTree();
      }

      this.lastComponentTreeHash = currentHash;
    }).catch(err => {
      console.error('Error checking component tree:', err);
    });
  }

  simpleHash(str) {
    let hash = 0;
    for (let i = 0; i < str.length; i++) {
      const char = str.charCodeAt(i);
      hash = ((hash << 5) - hash) + char;
      hash = hash & hash;
    }
    return hash;
  }

  refreshComponents() {
    if (!this.componentsSection || this.componentsSection.style.display === 'none') {
      this.checkComponentDebugMode();
      return;
    }
    this.getComponentTree();
  }

  getComponentTree() {
    this.evalInPage(`
      (function() {
        const Module = window.picorubyModule;
        const jsonStr = Module.ccall('mrb_get_component_debug_info', 'string', ['string'], ['component_tree']);
        return JSON.parse(jsonStr);
      })()
    `).then(response => {
      if (!response.error) {
        const components = typeof response.result === 'string' ? JSON.parse(response.result) : response.result;
        this.displayComponentTree(components);
      } else {
        console.error('Failed to get component tree:', response.error);
      }
    }).catch(err => {
      console.error('getComponentTree error:', err);
    });
  }

  displayComponentTree(components) {
    if (!this.componentTree) return;

    if (components.length === 0) {
      this.componentTree.innerHTML = '<div class="empty-state">No components</div>';
      return;
    }

    // Build component map for quick lookup
    const componentMap = {};
    components.forEach(comp => {
      componentMap[comp.id] = comp;
    });

    // Find root components (components that are not children of any other component)
    const allChildIds = new Set();
    components.forEach(comp => {
      if (comp.children) {
        comp.children.forEach(childId => allChildIds.add(childId));
      }
    });

    const rootComponents = components.filter(comp => !allChildIds.has(comp.id));

    // Render tree recursively
    const renderComponent = (comp, depth = 0) => {
      const hasChildren = comp.children && comp.children.length > 0;
      const isExpanded = this.expandedComponents.has(comp.id);

      let html = `
        <div class="component-item ${comp.id === this.selectedComponentId ? 'selected' : ''}"
             data-component-id="${comp.id}"
             style="padding-left: ${depth * 20}px;">
          ${hasChildren ? `<span class="tree-toggle ${isExpanded ? 'expanded' : ''}" data-component-id="${comp.id}">${isExpanded ? '▼' : '▶'}</span>` : '<span class="tree-spacer"></span>'}
          <span class="component-class">${this.escapeHtml(comp.class)}</span>
          <span class="component-id">#${comp.id}</span>
          ${comp.mounted ? '<span class="component-status">●</span>' : '<span class="component-status-unmounted">○</span>'}
        </div>
      `;

      if (hasChildren && isExpanded) {
        comp.children.forEach(childId => {
          const child = componentMap[childId];
          if (child) {
            html += renderComponent(child, depth + 1);
          }
        });
      }

      return html;
    };

    this.componentTree.innerHTML = rootComponents.map(comp => renderComponent(comp)).join('');

    // Add click listeners for component items
    this.componentTree.querySelectorAll('.component-item').forEach(item => {
      item.addEventListener('click', (e) => {
        // Don't select if clicking on toggle
        if (e.target.classList.contains('tree-toggle')) return;
        const componentId = parseInt(item.dataset.componentId);
        this.selectComponent(componentId);
      });
    });

    // Add click listeners for tree toggles
    this.componentTree.querySelectorAll('.tree-toggle').forEach(toggle => {
      toggle.addEventListener('click', (e) => {
        e.stopPropagation();
        const componentId = parseInt(toggle.dataset.componentId);
        if (this.expandedComponents.has(componentId)) {
          this.expandedComponents.delete(componentId);
        } else {
          this.expandedComponents.add(componentId);
        }
        this.getComponentTree();
      });
    });
  }

  selectComponent(componentId) {
    this.selectedComponentId = componentId;

    // Highlight the selected component in the DOM
    this.highlightComponent(componentId);

    this.inspectComponent(componentId);
    // Refresh tree to update selection UI
    this.getComponentTree();
  }

  highlightComponent(componentId) {
    this.evalInPage(`
      (function() {
        const cid = ${componentId};

        // Clear previous highlights
        document.querySelectorAll('.picoruby-devtools-selected').forEach(function(el) {
          el.classList.remove('picoruby-devtools-selected');
        });

        // Highlight the selected component
        const selector = '[data-component-id="' + cid + '"]';
        const elements = document.querySelectorAll(selector);
        elements.forEach(function(el) {
          el.classList.add('picoruby-devtools-selected');
        });

        // Scroll into view if possible
        if (elements.length > 0) {
          elements[0].scrollIntoView({ behavior: 'smooth', block: 'center' });
        }
      })()
    `).catch(err => {
      console.error('highlightComponent error:', err);
    });
  }

  clearHighlight() {
    this.evalInPage(`
      (function() {
        document.querySelectorAll('.picoruby-devtools-selected').forEach(function(el) {
          el.classList.remove('picoruby-devtools-selected');
        });
      })()
    `).catch(err => {
      console.error('clearHighlight error:', err);
    });
  }

  inspectComponent(componentId) {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const cid = ${componentId};
          const jsonStr = Module.ccall('mrb_get_component_state_by_id', 'string', ['number'], [cid]);
          const rubyData = JSON.parse(jsonStr);

        // Get DOM element info
        const selector = '[data-component-id="' + cid + '"]';
        const elements = document.querySelectorAll(selector);
        const domInfo = [];

        function getChildrenInfo(element, maxDepth, currentDepth, maxChildren) {
          maxDepth = maxDepth || 2;
          currentDepth = currentDepth || 0;
          maxChildren = maxChildren || 20;

          if (currentDepth >= maxDepth) return null;

          const childrenArray = Array.from(element.children);
          const limitedChildren = childrenArray.slice(0, maxChildren);
          const hasMore = childrenArray.length > maxChildren;

          const children = limitedChildren.map(function(child) {
            const childInfo = {
              tagName: child.tagName.toLowerCase(),
              id: child.id || null,
              className: child.className ? child.className.substring(0, 100) : null,
              textContent: child.childNodes.length === 1 && child.childNodes[0].nodeType === 3
                ? child.textContent.trim().substring(0, 50)
                : null,
              hasChildren: child.children.length > 0,
              childCount: child.children.length
            };

            if (child.children.length > 0 && currentDepth + 1 < maxDepth) {
              childInfo.children = getChildrenInfo(child, maxDepth, currentDepth + 1, maxChildren);
            }

            return childInfo;
          });

          if (hasMore) {
            children.push({
              tagName: 'more',
              textContent: '... ' + (childrenArray.length - maxChildren) + ' more children',
              isPlaceholder: true
            });
          }

          return children.length > 0 ? children : null;
        }

        elements.forEach(function(el) {
          var attrs = Array.from(el.attributes);
          var limitedAttrs = attrs.slice(0, 30).map(function(attr) {
            return {
              name: attr.name,
              value: attr.value.substring(0, 200)
            };
          });

          domInfo.push({
            tagName: el.tagName.toLowerCase(),
            id: el.id || null,
            className: el.className ? el.className.substring(0, 200) : null,
            attributes: limitedAttrs,
            attributeCount: attrs.length,
            childCount: el.children.length,
            children: getChildrenInfo(el)
          });
        });

          return {
            ruby: rubyData,
            dom: domInfo
          };
        } catch(e) {
          return {
            error: e.toString(),
            message: e.message,
            stack: e.stack
          };
        }
      })()
    `).then(response => {
      console.log('inspectComponent response:', response);
      if (response && response.ruby) {
        this.displayComponentState(componentId, response.ruby, response.dom || []);
      } else if (response && !response.error) {
        this.displayComponentState(componentId, response, []);
      } else {
        console.error('Failed to inspect component:', response ? response.error : 'No response');
        this.componentInspector.innerHTML = '<div class="empty-state">Error loading component data</div>';
      }
    }).catch(err => {
      console.error('inspectComponent error:', err);
      this.componentInspector.innerHTML = '<div class="empty-state">Error: ' + err.message + '</div>';
    });
  }

  displayComponentState(componentId, rubyData, domInfo) {
    if (!this.componentInspector) return;

    console.log('displayComponentState called:', { componentId, rubyData, domInfo });

    // Handle case where rubyData might be the direct result
    const data = rubyData.result || rubyData;
    const stateEntries = Object.entries(data.state || {});
    const ivarEntries = Object.entries(data.ivars || {});

    let html = `<div class="component-inspector-content">`;
    html += `<h4>Component #${componentId}</h4>`;

    // Display DOM elements FIRST (highest priority)
    const safedomInfo = domInfo || [];
    if (safedomInfo.length > 0) {
      html += `<div class="inspector-section"><strong>DOM Elements:</strong></div>`;
      safedomInfo.forEach((el, idx) => {
        html += `<div style="margin-bottom: 12px; padding: 8px; background: #1e1e1e; border-radius: 4px;">`;
        html += `<div style="color: #4ec9b0; margin-bottom: 4px; font-size: 12px;">&lt;${this.escapeHtml(el.tagName)}&gt;</div>`;
        if (el.id) {
          html += `<div style="font-size: 11px; color: #888; margin-bottom: 2px;">id: <span style="color: #ce9178;">${this.escapeHtml(el.id)}</span></div>`;
        }
        if (el.className) {
          html += `<div style="font-size: 11px; color: #888; margin-bottom: 2px;">class: <span style="color: #ce9178;">${this.escapeHtml(el.className)}</span></div>`;
        }

        html += `<details style="margin-top: 8px;">`;
        const attrCount = el.attributeCount || el.attributes.length;
        html += `<summary style="cursor: pointer; color: #888; font-size: 10px;">Attributes (${attrCount})`;
        if (el.attributeCount && el.attributeCount > el.attributes.length) {
          html += ` <span style="color: #666;">- showing first ${el.attributes.length}</span>`;
        }
        html += `</summary>`;
        html += `<div style="margin-top: 4px; font-family: 'Menlo', 'Monaco', 'Courier New', monospace; font-size: 10px;">`;
        el.attributes.forEach(attr => {
          html += `<div style="color: #9cdcfe; padding: 2px 0;">${this.escapeHtml(attr.name)}<span style="color: #888;">="</span><span style="color: #ce9178;">${this.escapeHtml(attr.value)}</span><span style="color: #888;">"</span></div>`;
        });
        html += `</div></details>`;

        if (el.children && el.children.length > 0) {
          html += `<details style="margin-top: 8px;" open>`;
          html += `<summary style="cursor: pointer; color: #888; font-size: 10px;">Children (${el.childCount})</summary>`;
          html += `<div style="margin-top: 4px; margin-left: 12px;">`;
          html += this.renderDOMChildren(el.children, 0);
          html += `</div></details>`;
        }

        html += `</div>`;
      });
    }

    // Display state
    if (stateEntries.length > 0) {
      html += `<div class="inspector-section"><strong>State:</strong></div>`;
      html += `<ul class="variable-list">`;
      stateEntries.forEach(([key, value]) => {
        html += `
          <li class="variable-item">
            <span class="variable-name">${this.escapeHtml(key)}</span>
            <span> = </span>
            <span class="variable-value">${this.escapeHtml(value)}</span>
          </li>
        `;
      });
      html += `</ul>`;
    }

    // Display instance variables
    if (ivarEntries.length > 0) {
      html += `<div class="inspector-section"><strong>Instance Variables:</strong></div>`;
      html += `<ul class="variable-list">`;
      ivarEntries.forEach(([key, value]) => {
        html += `
          <li class="variable-item">
            <span class="variable-name">${this.escapeHtml(key)}</span>
            <span> = </span>
            <span class="variable-value">${this.escapeHtml(value)}</span>
          </li>
        `;
      });
      html += `</ul>`;
    }

    // Display Ruby inspect (raw data) - lowest priority
    if (data.inspect) {
      html += `<div class="inspector-section"><strong>Ruby Inspect:</strong></div>`;
      html += `<pre style="padding: 8px; background: #1e1e1e; margin-bottom: 12px; font-family: 'Menlo', 'Monaco', 'Courier New', monospace; font-size: 11px; color: #ce9178; border-radius: 4px; overflow-x: auto; white-space: pre-wrap; word-wrap: break-word;">`;
      html += this.escapeHtml(data.inspect);
      html += `</pre>`;
    }

    html += `</div>`;
    this.componentInspector.innerHTML = html;
  }

  renderDOMChildren(children, depth) {
    if (!children || children.length === 0) return '';

    let html = '';
    children.forEach(child => {
      if (child.isPlaceholder) {
        html += `<div style="margin: 4px 0; padding: 4px; color: #666; font-size: 10px; font-style: italic;">`;
        html += this.escapeHtml(child.textContent);
        html += `</div>`;
        return;
      }

      html += `<div style="margin: 4px 0; padding: 4px; background: #2d2d2d; border-radius: 2px; font-size: 10px;">`;
      html += `<span style="color: #4ec9b0;">&lt;${this.escapeHtml(child.tagName)}&gt;</span>`;

      if (child.id) {
        html += ` <span style="color: #888;">id=</span><span style="color: #ce9178;">"${this.escapeHtml(child.id)}"</span>`;
      }
      if (child.className) {
        html += ` <span style="color: #888;">class=</span><span style="color: #ce9178;">"${this.escapeHtml(child.className)}"</span>`;
      }
      if (child.textContent) {
        html += ` <span style="color: #d4d4d4;">${this.escapeHtml(child.textContent)}</span>`;
      }
      if (child.hasChildren) {
        html += ` <span style="color: #666;">(${child.childCount} children)</span>`;
      }

      if (child.children && child.children.length > 0) {
        html += `<div style="margin-left: 12px; margin-top: 4px;">`;
        html += this.renderDOMChildren(child.children, depth + 1);
        html += `</div>`;
      }

      html += `</div>`;
    });

    return html;
  }
}

const picorubyDebugger = new PicoRubyDebugger();
