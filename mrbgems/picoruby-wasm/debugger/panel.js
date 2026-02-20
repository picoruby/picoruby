// PicoRuby WASM Debugger - DevTools panel script

class PicoRubyDebugger {
  constructor() {
    this.status = document.getElementById('status');
    this.replOutput = document.getElementById('replOutput');
    this.replInput = document.getElementById('replInput');
    this.replPrompt = document.getElementById('replPrompt');
    this.localsContent = document.getElementById('localsContent');
    this.callstackContent = document.getElementById('callstackContent');

    // Component debug elements
    this.componentsPanel = document.getElementById('componentsPanel');
    this.componentTree = document.getElementById('componentTree');
    this.componentInspector = document.getElementById('componentInspector');

    this.isPaused = false;
    this.pauseId = -1;
    this.isConnected = false;
    this.debugPollInterval = null;
    this.selectedComponentId = null;
    this.expandedComponents = new Set();
    this.autoRefreshInterval = null;
    this.lastComponentTreeHash = null;
    this.lineNumber = 1;

    this.setupEventListeners();
    this.updatePrompt();
    this.checkConnection();
  }

  setupEventListeners() {
    this.replInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        this.evalCode(this.replInput.value);
        this.replInput.value = '';
      }
    });

    // Keyboard shortcuts
    document.addEventListener('keydown', (e) => {
      if (!this.isPaused) return;
      if (e.key === 'F8') {
        e.preventDefault();
        this.debugContinue();
      } else if (e.key === 'F11') {
        e.preventDefault();
        this.debugStep();
      } else if (e.key === 'F10') {
        e.preventDefault();
        this.debugNext();
      }
    });
  }

  checkConnection() {
    this.evalInPage('typeof window.picorubyModule')
      .then(result => {
        if (result === 'undefined') {
          this.updateStatus('PicoRuby Module not found');
          return;
        }

        this.isConnected = true;
        this.updateStatus('Connected to PicoRuby');
        this.startDebugPolling();
        this.checkComponentDebugMode();
      })
      .catch(err => {
        console.error('Connection check error:', err);
        this.updateStatus('Error: ' + err.message);
      });
  }

  // -- Debug status polling --

  startDebugPolling() {
    if (this.debugPollInterval) return;

    this.debugPollInterval = setInterval(() => {
      this.pollDebugStatus();
    }, 200);
  }

  stopDebugPolling() {
    if (this.debugPollInterval) {
      clearInterval(this.debugPollInterval);
      this.debugPollInterval = null;
    }
  }

  pollDebugStatus() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          if (!Module || typeof Module.ccall !== 'function') return null;
          if (typeof Module._mrb_debug_get_status === 'undefined') return null;
          const jsonStr = Module.ccall('mrb_debug_get_status', 'string', [], []);
          return JSON.parse(jsonStr);
        } catch(e) {
          return null;
        }
      })()
    `).then(status => {
      if (!status) return;
      console.log('[poll] status:', JSON.stringify(status),
                  'isPaused:', this.isPaused, 'pauseId:', this.pauseId);
      if (status.mode === 'paused') {
        const newPause = status.pause_id !== this.pauseId;
        if (!this.isPaused || newPause) {
          console.log('[poll] -> enterDebugMode (newPause:', newPause, ')');
          this.enterDebugMode(status);
        }
      } else if (this.isPaused) {
        console.log('[poll] -> exitDebugMode');
        this.exitDebugMode();
      }
    }).catch(() => {});
  }

  enterDebugMode(status) {
    console.log('[enterDebugMode] pause_id:', status.pause_id,
                'file:', status.file, 'line:', status.line);
    this.isPaused = true;
    this.pauseId = status.pause_id;
    this.updatePrompt();
    const file = status.file || '(unknown)';
    const line = status.line || 0;
    this.updateStatus(`Paused at ${file}:${line}`, true);

    // Show pause info in REPL
    this.appendReplInfo(`-- Paused at ${file}:${line} --`);

    // Fetch locals and callstack
    this.fetchLocals();
    this.fetchCallstack();
  }

  exitDebugMode() {
    console.log('[exitDebugMode] was isPaused:', this.isPaused);
    this.isPaused = false;
    this.updatePrompt();
    this.updateStatus('Connected to PicoRuby');
    this.localsContent.innerHTML = '<div class="empty-state">Not paused</div>';
    this.callstackContent.innerHTML = '<div class="empty-state">Not paused</div>';
  }

  // -- Debug actions --

  /* Handle the response from continue/step/next.
   * If the task synchronously re-paused, the C function returns the
   * paused status directly (mode === 'paused'); otherwise it returns
   * a running/stepping status and we fall back to polling. */
  handleDebugActionResult(result) {
    console.log('[handleDebugActionResult] result:', JSON.stringify(result));
    if (result.mode === 'paused') {
      console.log('[handleDebugActionResult] -> enterDebugMode');
      this.enterDebugMode(result);
    } else {
      console.log('[handleDebugActionResult] -> pollDebugStatus');
      this.pollDebugStatus();
    }
  }

  debugContinue() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const jsonStr = Module.ccall('mrb_debug_continue', 'string', [], []);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(result => {
      console.log('[debugContinue] result:', JSON.stringify(result));
      if (result.error) {
        this.appendReplError('Continue error: ' + result.error);
      } else {
        this.appendReplInfo('-- Continued --');
        this.handleDebugActionResult(result);
      }
    }).catch(err => {
      console.error('[debugContinue] catch:', err);
      this.appendReplError('Continue error: ' + err.message);
    });
  }

  debugStep() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const jsonStr = Module.ccall('mrb_debug_step', 'string', [], []);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(result => {
      if (result.error) {
        this.appendReplError('Step error: ' + result.error);
      } else {
        this.handleDebugActionResult(result);
      }
    }).catch(err => {
      this.appendReplError('Step error: ' + err.message);
    });
  }

  debugNext() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const jsonStr = Module.ccall('mrb_debug_next', 'string', [], []);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(result => {
      if (result.error) {
        this.appendReplError('Next error: ' + result.error);
      } else {
        this.handleDebugActionResult(result);
      }
    }).catch(err => {
      this.appendReplError('Next error: ' + err.message);
    });
  }

  // -- Locals & Callstack --

  fetchLocals() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const jsonStr = Module.ccall('mrb_debug_get_locals', 'string', [], []);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(locals => {
      if (locals.error) {
        this.localsContent.innerHTML =
          '<div class="empty-state">' + this.escapeHtml(locals.error) + '</div>';
        return;
      }
      this.displayLocals(locals);
    }).catch(() => {
      this.localsContent.innerHTML = '<div class="empty-state">Failed to load</div>';
    });
  }

  displayLocals(locals) {
    let html = '';
    for (const [name, value] of Object.entries(locals)) {
      html += `
        <div class="variable-item">
          <span class="variable-name">${this.escapeHtml(name)}</span>
          <span class="variable-sep">=</span>
          <span class="variable-value">${this.escapeHtml(value)}</span>
        </div>`;
    }
    if (!html) {
      html = '<div class="empty-state">No local variables</div>';
    }
    this.localsContent.innerHTML = html;
  }

  fetchCallstack() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const jsonStr = Module.ccall('mrb_debug_get_callstack', 'string', [], []);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(stack => {
      if (stack.error) {
        this.callstackContent.innerHTML =
          '<div class="empty-state">' + this.escapeHtml(stack.error) + '</div>';
        return;
      }
      this.displayCallstack(stack);
    }).catch(() => {
      this.callstackContent.innerHTML = '<div class="empty-state">Failed to load</div>';
    });
  }

  displayCallstack(frames) {
    if (!Array.isArray(frames) || frames.length === 0) {
      this.callstackContent.innerHTML = '<div class="empty-state">No frames</div>';
      return;
    }

    let html = '';
    frames.forEach((frame, idx) => {
      const cls = idx === 0 ? 'stack-frame active' : 'stack-frame';
      html += `
        <div class="${cls}">
          <div class="stack-frame-method">${this.escapeHtml(frame.method)}</div>
          <div class="stack-frame-location">${this.escapeHtml(frame.file)}:${frame.line}</div>
        </div>`;
    });
    this.callstackContent.innerHTML = html;
  }

  // -- REPL --

  parseDebugCommand(input) {
    const cmd = input.trim();
    switch (cmd) {
      case 'c': case 'continue': return 'continue';
      case 's': case 'step':     return 'step';
      case 'n': case 'next':     return 'next';
      case 'h': case 'help':     return 'help';
      default:                   return null;
    }
  }

  executeDebugCommand(cmd, entry) {
    switch (cmd) {
      case 'continue': {
        const info = document.createElement('div');
        info.className = 'repl-info';
        info.textContent = '=> continue';
        entry.appendChild(info);
        this.debugContinue();
        break;
      }
      case 'step': {
        const info = document.createElement('div');
        info.className = 'repl-info';
        info.textContent = '=> step';
        entry.appendChild(info);
        this.debugStep();
        break;
      }
      case 'next': {
        const info = document.createElement('div');
        info.className = 'repl-info';
        info.textContent = '=> next';
        entry.appendChild(info);
        this.debugNext();
        break;
      }
      case 'help': {
        const lines = [
          'Debug commands:',
          '  c, continue  - Resume execution',
          '  s, step      - Step into',
          '  n, next      - Step over',
          '  h, help      - Show this help',
          '',
          'Keyboard shortcuts:',
          '  F8  - Continue',
          '  F11 - Step into',
          '  F10 - Step over',
        ];
        lines.forEach(line => {
          const info = document.createElement('div');
          info.className = 'repl-info';
          info.textContent = line || '\u00A0';
          entry.appendChild(info);
        });
        break;
      }
    }
    this.replOutput.scrollTop = this.replOutput.scrollHeight;
  }

  getPromptText() {
    const num = String(this.lineNumber).padStart(3, '0');
    return this.isPaused ? `irb(debug):${num}> ` : `irb:${num}> `;
  }

  updatePrompt() {
    this.replPrompt.textContent = this.getPromptText();
    if (this.isPaused) {
      this.replPrompt.classList.add('debug');
    } else {
      this.replPrompt.classList.remove('debug');
    }
  }

  evalCode(code) {
    if (!code.trim()) return;

    const promptText = this.getPromptText();
    const entry = document.createElement('div');
    entry.className = 'repl-entry';
    entry.innerHTML = `<div class="repl-input-line">${this.escapeHtml(promptText)}${this.escapeHtml(code)}</div>`;

    this.clearEmptyState();
    this.replOutput.appendChild(entry);

    this.lineNumber++;
    this.updatePrompt();

    // Check for debug commands when paused
    if (this.isPaused) {
      const cmd = this.parseDebugCommand(code);
      if (cmd) {
        this.executeDebugCommand(cmd, entry);
        return;
      }
    }

    // Use debug eval when paused, otherwise normal eval
    const evalFn = this.isPaused ? 'mrb_debug_eval_in_binding' : 'mrb_eval_string';
    console.log('[evalCode] isPaused:', this.isPaused, 'evalFn:', evalFn, 'code:', code);

    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined') {
          return { error: 'PicoRuby not available' };
        }
        try {
          const jsonStr = window.picorubyModule.ccall(
            '${evalFn}', 'string', ['string'], [${JSON.stringify(code)}]);
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
        outputLine.textContent = '=> ' + response.result;
        entry.appendChild(outputLine);
      }
      this.replOutput.scrollTop = this.replOutput.scrollHeight;

      // Refresh locals after eval in debug mode
      if (this.isPaused) {
        this.fetchLocals();
      }
    }).catch(err => {
      const errorLine = document.createElement('div');
      errorLine.className = 'repl-error';
      errorLine.textContent = 'DevTools error: ' + err.message;
      entry.appendChild(errorLine);
      this.replOutput.scrollTop = this.replOutput.scrollHeight;
    });
  }

  // -- Helper methods --

  appendReplInfo(message) {
    this.clearEmptyState();
    const div = document.createElement('div');
    div.className = 'repl-info';
    div.textContent = message;
    this.replOutput.appendChild(div);
    this.replOutput.scrollTop = this.replOutput.scrollHeight;
  }

  appendReplError(message) {
    this.clearEmptyState();
    const div = document.createElement('div');
    div.className = 'repl-error';
    div.textContent = message;
    this.replOutput.appendChild(div);
    this.replOutput.scrollTop = this.replOutput.scrollHeight;
  }

  clearEmptyState() {
    const empty = this.replOutput.querySelector('.empty-state');
    if (empty) {
      this.replOutput.innerHTML = '';
    }
  }

  updateStatus(message, paused) {
    this.status.textContent = message;
    if (paused) {
      this.status.classList.add('paused');
    } else {
      this.status.classList.remove('paused');
    }
  }

  evalInPage(code) {
    return new Promise((resolve, reject) => {
      try {
        chrome.devtools.inspectedWindow.eval(code, (result, exceptionInfo) => {
          if (exceptionInfo) {
            reject(new Error(exceptionInfo.description || 'Eval failed'));
          } else {
            resolve(result);
          }
        });
      } catch (err) {
        reject(err);
      }
    });
  }

  escapeHtml(text) {
    if (typeof text !== 'string') return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  // -- Component Debug Mode (Funicular) --

  checkComponentDebugMode() {
    this.evalInPage(`
      (function() {
        if (typeof window.picorubyModule === 'undefined') return { available: false };
        const Module = window.picorubyModule;
        try {
          const code = "global_variables.include?(:$__funicular_debug__) ? 'enabled' : 'disabled'";
          const jsonStr = Module.ccall('mrb_eval_string', 'string', ['string'], [code]);
          const result = JSON.parse(jsonStr);
          return { available: result.result === '"enabled"' };
        } catch(e) {
          return { available: false };
        }
      })()
    `).then(result => {
      if (result.available) {
        this.enableComponentDebug();
      }
    }).catch(() => {});
  }

  enableComponentDebug() {
    if (!this.componentsPanel) return;
    this.componentsPanel.classList.add('visible');
    setTimeout(() => {
      this.refreshComponents();
      this.startComponentAutoRefresh();
    }, 100);
  }

  startComponentAutoRefresh() {
    if (this.autoRefreshInterval) {
      clearInterval(this.autoRefreshInterval);
    }
    this.autoRefreshInterval = setInterval(() => {
      this.checkAndRefreshComponents();
    }, 500);
  }

  checkAndRefreshComponents() {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          if (!Module || typeof Module.ccall !== 'function') return null;
          return Module.ccall('mrb_get_component_debug_info', 'string',
                              ['string'], ['component_tree']);
        } catch (e) {
          return null;
        }
      })()
    `).then(response => {
      if (!response) return;

      const currentHash = this.simpleHash(response);
      if (this.lastComponentTreeHash !== null &&
          this.lastComponentTreeHash !== currentHash) {
        this.getComponentTree();
      }
      this.lastComponentTreeHash = currentHash;
    }).catch(() => {});
  }

  simpleHash(str) {
    let hash = 0;
    for (let i = 0; i < str.length; i++) {
      const c = str.charCodeAt(i);
      hash = ((hash << 5) - hash) + c;
      hash = hash & hash;
    }
    return hash;
  }

  refreshComponents() {
    if (!this.componentsPanel ||
        !this.componentsPanel.classList.contains('visible')) {
      return;
    }
    this.getComponentTree();
  }

  getComponentTree() {
    this.evalInPage(`
      (function() {
        const Module = window.picorubyModule;
        const jsonStr = Module.ccall('mrb_get_component_debug_info', 'string',
                                     ['string'], ['component_tree']);
        return JSON.parse(jsonStr);
      })()
    `).then(response => {
      if (!response.error) {
        const components = typeof response.result === 'string'
          ? JSON.parse(response.result)
          : response.result;
        this.displayComponentTree(components);
      }
    }).catch(() => {});
  }

  displayComponentTree(components) {
    if (!this.componentTree) return;

    if (!components || components.length === 0) {
      this.componentTree.innerHTML = '<div class="empty-state">No components</div>';
      return;
    }

    const componentMap = {};
    components.forEach(comp => { componentMap[comp.id] = comp; });

    const allChildIds = new Set();
    components.forEach(comp => {
      if (comp.children) {
        comp.children.forEach(childId => allChildIds.add(childId));
      }
    });

    const rootComponents = components.filter(comp => !allChildIds.has(comp.id));

    const renderComponent = (comp, depth) => {
      depth = depth || 0;
      const hasChildren = comp.children && comp.children.length > 0;
      const isExpanded = this.expandedComponents.has(comp.id);
      const isSelected = comp.id === this.selectedComponentId;

      let html = `
        <div class="component-item ${isSelected ? 'selected' : ''}"
             data-component-id="${comp.id}"
             style="padding-left: ${depth * 20}px;">
          ${hasChildren
            ? `<span class="tree-toggle ${isExpanded ? 'expanded' : ''}"
                     data-component-id="${comp.id}">${isExpanded ? '\u25BC' : '\u25B6'}</span>`
            : '<span class="tree-spacer"></span>'}
          <span class="component-class">${this.escapeHtml(comp.class)}</span>
          <span class="component-id">#${comp.id}</span>
          ${comp.mounted
            ? '<span class="component-status">\u25CF</span>'
            : '<span class="component-status-unmounted">\u25CB</span>'}
        </div>`;

      if (hasChildren && isExpanded) {
        comp.children.forEach(childId => {
          const child = componentMap[childId];
          if (child) html += renderComponent(child, depth + 1);
        });
      }
      return html;
    };

    this.componentTree.innerHTML = rootComponents
      .map(comp => renderComponent(comp)).join('');

    // Event listeners for component items
    this.componentTree.querySelectorAll('.component-item').forEach(item => {
      item.addEventListener('click', (e) => {
        if (e.target.classList.contains('tree-toggle')) return;
        const componentId = parseInt(item.dataset.componentId);
        this.selectComponent(componentId, components);
      });
    });

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

  selectComponent(componentId, components) {
    if (this.selectedComponentId === componentId) {
      this.selectedComponentId = null;
      this.componentInspector.innerHTML =
        '<div class="empty-state">Select a component</div>';
      this.getComponentTree();
      return;
    }

    this.selectedComponentId = componentId;
    const component = components.find(c => c.id === componentId);
    if (component && component.children && component.children.length > 0) {
      this.expandedComponents.add(componentId);
    }

    this.inspectComponent(componentId);
    this.getComponentTree();
  }

  inspectComponent(componentId) {
    this.evalInPage(`
      (function() {
        try {
          const Module = window.picorubyModule;
          const jsonStr = Module.ccall('mrb_get_component_state_by_id', 'string',
                                       ['number'], [${componentId}]);
          return JSON.parse(jsonStr);
        } catch(e) {
          return { error: e.toString() };
        }
      })()
    `).then(response => {
      if (response && !response.error) {
        this.displayComponentState(componentId, response.result || response);
      } else {
        this.componentInspector.innerHTML =
          '<div class="empty-state">Error loading component data</div>';
      }
    }).catch(() => {
      this.componentInspector.innerHTML =
        '<div class="empty-state">Error loading component data</div>';
    });
  }

  displayComponentState(componentId, data) {
    if (!this.componentInspector) return;

    const stateEntries = Object.entries(data.state || {});
    const ivarEntries = Object.entries(data.ivars || {});

    let html = `<div style="padding: 4px;"><strong>Component #${componentId}</strong>`;

    if (stateEntries.length > 0) {
      html += '<div class="inspector-section">State</div>';
      stateEntries.forEach(([key, value]) => {
        html += `
          <div class="variable-item">
            <span class="variable-name">${this.escapeHtml(key)}</span>
            <span class="variable-sep">=</span>
            <span class="variable-value">${this.escapeHtml(value)}</span>
          </div>`;
      });
    }

    if (ivarEntries.length > 0) {
      html += '<div class="inspector-section">Instance Variables</div>';
      ivarEntries.forEach(([key, value]) => {
        html += `
          <div class="variable-item">
            <span class="variable-name">${this.escapeHtml(key)}</span>
            <span class="variable-sep">=</span>
            <span class="variable-value">${this.escapeHtml(value)}</span>
          </div>`;
      });
    }

    html += '</div>';
    this.componentInspector.innerHTML = html;
  }
}

const picorubyDebugger = new PicoRubyDebugger();
