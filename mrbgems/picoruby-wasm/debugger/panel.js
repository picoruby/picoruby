// PicoRuby DevTools panel script

class PicoRubyDebugger {
  constructor() {
    this.status = document.getElementById('status');
    this.callStack = document.getElementById('callStack');
    this.variables = document.getElementById('variables');
    this.globals = document.getElementById('globals');
    this.replOutput = document.getElementById('replOutput');
    this.replInput = document.getElementById('replInput');

    // Component debug elements
    this.componentsSection = document.getElementById('componentsSection');
    this.componentTree = document.getElementById('componentTree');
    this.componentInspector = document.getElementById('componentInspector');

    this.isPaused = false;
    this.selectedComponentId = null;
    this.expandedComponents = new Set();
    this.setupEventListeners();
    this.checkConnection();
  }

  setupEventListeners() {
    document.getElementById('refreshBtn').addEventListener('click', () => {
      this.refresh();
    });

    document.getElementById('pauseBtn').addEventListener('click', () => {
      this.pause();
    });

    document.getElementById('resumeBtn').addEventListener('click', () => {
      this.resume();
    });

    this.replInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        this.evalCode(this.replInput.value);
        this.replInput.value = '';
      }
    });

    // Message listener to receive notifications from WASM
    window.addEventListener('message', (event) => {
      if (event.data && event.data.type === 'picoruby-event') {
        this.handlePicoRubyEvent(event.data);
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

  refresh() {
    this.updateStatus('Refreshing...');

    this.getCallStack();

    this.getVariables();

    this.getGlobals();

    this.refreshComponents();

    this.updateStatus('Ready');
  }

  getCallStack() {
    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined' || !window.picorubyModule._mrb_get_backtrace) {
          return null;
        }
        try {
          // TODO: Implement in C side
          // const ptr = window.picorubyModule._mrb_get_backtrace();
          // return window.picorubyModule.UTF8ToString(ptr);
          return JSON.stringify([
            { method: 'main', file: 'app.rb', line: 1 }
          ]);
        } catch(e) {
          return null;
        }
      })()
    `).then(result => {
      if (result) {
        const frames = JSON.parse(result);
        this.displayCallStack(frames);
      } else {
        this.callStack.innerHTML = '<li class="empty-state">Stack trace not available</li>';
      }
    }).catch(err => console.error("getCallStack error:", err));
  }

  displayCallStack(frames) {
    if (frames.length === 0) {
      this.callStack.innerHTML = '<li class="empty-state">No stack frames</li>';
      return;
    }

    this.callStack.innerHTML = frames.map((frame, i) => `
      <li class="stack-frame ${i === 0 ? 'active' : ''}" data-frame="${i}">
        <div class="stack-frame-method">${this.escapeHtml(frame.method)}</div>
        <div class="stack-frame-location">${this.escapeHtml(frame.file)}:${frame.line}</div>
      </li>
    `).join('');
  }

  getVariables() {
    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined' || !window.picorubyModule._mrb_get_local_variables) {
          return null;
        }
        try {
          // TODO: Implement in C side
          // const ptr = window.picorubyModule._mrb_get_local_variables();
          // return window.picorubyModule.UTF8ToString(ptr);
          return JSON.stringify({
            x: '42',
            name: '"hello"',
            array: '[1, 2, 3]'
          }).catch(err => console.error("getVariables error:", err));
        } catch(e) {
          return null;
        }
      })()
    `).then(result => {
      if (result) {
        const vars = JSON.parse(result);
        this.displayVariables(vars);
      } else {
        this.variables.innerHTML = '<li class="empty-state">Variables not available</li>';
      }
    }).catch(err => console.error("getVariables error:", err));
  }

  displayVariables(vars) {
    const entries = Object.entries(vars);
    if (entries.length === 0) {
      this.variables.innerHTML = '<li class="empty-state">No local variables</li>';
      return;
    }

    this.variables.innerHTML = entries.map(([name, value]) => `
      <li class="variable-item">
        <span class="variable-name">${this.escapeHtml(name)}</span>
        <span> = </span>
        <span class="variable-value">${this.escapeHtml(value)}</span>
      </li>
    `).join('');
  }

  getGlobals() {
    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined') {
          return null;
        }
        try {
          const jsonStr = window.picorubyModule.ccall('mrb_get_globals_json', 'string', [], []);
          return jsonStr;
        } catch(e) {
          return null;
        }
      })()
    `).then(result => {
      if (result) {
        try {
          const vars = JSON.parse(result);
          this.displayGlobals(vars);
        } catch(e) {
          this.globals.innerHTML = '<li class="empty-state">Error parsing globals</li>';
        }
      } else {
        this.globals.innerHTML = '<li class="empty-state">Globals not available</li>';
      }
    }).catch(err => console.error("getGlobals error:", err));
  }

  displayGlobals(vars) {
    const entries = Object.entries(vars);
    if (entries.length === 0) {
      this.globals.innerHTML = '<li class="empty-state">No global variables</li>';
      return;
    }

    this.globals.innerHTML = entries.map(([name, value]) => `
      <li class="variable-item">
        <span class="variable-name">${this.escapeHtml(name)}</span>
        <span> = </span>
        <span class="variable-value">${this.escapeHtml(value)}</span>
      </li>
    `).join('');
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

  pause() {
    this.evalInPage(`
      if (typeof window.picorubyModule !== 'undefined' && window.picorubyModule._mrb_debug_pause) {
        window.picorubyModule._mrb_debug_pause();
      }
    `);
    this.isPaused = true;
    document.getElementById('pauseBtn').disabled = true;
    document.getElementById('resumeBtn').disabled = false;
    this.updateStatus('Paused');
  }

  resume() {
    this.evalInPage(`
      if (typeof window.picorubyModule !== 'undefined' && window.picorubyModule._mrb_debug_resume) {
        window.picorubyModule._mrb_debug_resume();
      }
    `);
    this.isPaused = false;
    document.getElementById('pauseBtn').disabled = false;
    document.getElementById('resumeBtn').disabled = true;
    this.updateStatus('Running');
  }

  handlePicoRubyEvent(data) {
    switch(data.event) {
      case 'breakpoint':
        this.isPaused = true;
        this.updateStatus(`Paused at ${data.file}:${data.line}`);
        this.refresh();
        break;
      case 'exception':
        this.updateStatus(`Exception: ${data.message}`);
        break;
    }
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
    if (!this.componentsSection) return;
    this.componentsSection.style.display = 'flex';
    this.updateStatus('Component Debug enabled ✓');
    setTimeout(() => {
      this.refreshComponents();
    }, 100);
  }

  showComponentDebugUnavailable(reason) {
    if (!this.componentsSection) return;
    this.componentsSection.style.display = 'none';
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
    this.inspectComponent(componentId);
    // Refresh tree to update selection UI
    this.getComponentTree();
  }

  inspectComponent(componentId) {
    this.evalInPage(`
      (function() {
        const Module = window.picorubyModule;
        const jsonStr = Module.ccall('mrb_get_component_state_by_id', 'string', ['number'], [${componentId}]);
        return JSON.parse(jsonStr);
      })()
    `).then(response => {
      if (!response.error) {
        this.displayComponentState(componentId, response.result);
      } else {
        console.error('Failed to inspect component:', response.error);
      }
    }).catch(err => {
      console.error('inspectComponent error:', err);
    });
  }

  displayComponentState(componentId, data) {
    if (!this.componentInspector) return;

    const stateEntries = Object.entries(data.state || {});
    const ivarEntries = Object.entries(data.ivars || {});

    let html = `<div class="component-inspector-content">`;
    html += `<h4>Component #${componentId}</h4>`;

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

    if (stateEntries.length === 0 && ivarEntries.length === 0) {
      html += `<div class="empty-state">No state or variables</div>`;
    }

    html += `</div>`;
    this.componentInspector.innerHTML = html;
  }
}

const picorubyDebugger = new PicoRubyDebugger();
