// PicoRuby DevTools panel script

class PicoRubyDebugger {
  constructor() {
    this.status = document.getElementById('status');
    this.callStack = document.getElementById('callStack');
    this.variables = document.getElementById('variables');
    this.globals = document.getElementById('globals');
    this.replOutput = document.getElementById('replOutput');
    this.replInput = document.getElementById('replInput');

    this.isPaused = false;
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
        console.log('typeof window.picorubyModule:', result);
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
        console.log('PicoRuby Module info:', info);

        if (info.hasModule) {
          this.updateStatus('Connected to PicoRuby ✓');
          this.displayConnectionInfo(info);
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
}

const picorubyDebugger = new PicoRubyDebugger();
