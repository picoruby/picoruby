(async function(global) {
  async function initPicoRuby() {
    // Import the factory function
    const baseURL = document.currentScript?.src ? new URL('.', document.currentScript.src).href : '';
    const { default: createModule } = await import(baseURL + 'picoruby.js');

    async function collectRubyScripts() {
      const rubyScripts = document.querySelectorAll('script[type="text/ruby"]');
      const taskPromises = Array.from(rubyScripts).map(async script => {
        if (script.src) {
          const response = await fetch(script.src);
          if (!response.ok) {
            throw new Error(`Failed to load ${script.src}: ${response.statusText}`);
          }
          return await response.text();
        }
        return script.textContent.trim();
      });
      return Promise.all(taskPromises);
    }

    async function collectMrbVMCode() {
      const mrbScripts = document.querySelectorAll('script[type="application/x-mrb"]');
      const taskPromises = Array.from(mrbScripts).map(async script => {
        const src = script.src;
        if (src) {
          const response = await fetch(src);
          if (!response.ok) {
            throw new Error(`Failed to load ${src}: ${response.statusText}`);
          }
          return await response.arrayBuffer();
        }
        return null;
      });
      const results = await Promise.all(taskPromises);
      return results.filter(Boolean);
    }

    // Create and initialize the module
    const Module = await createModule();

    // Expose Module for debugging (used by PicoRuby DevTools extension)
    window.picorubyModule = Module;

    Module.picorubyRun = function() {
      const tickTimer = setInterval(() => {
        Module.ccall('mrb_tick_wasm', null, [], []);
      }, 17); // 16.67ms: optimize for 60fps

      function run() {
        const result = Module.ccall('mrb_run_step', 'number', [], []);
        if (result < 0) {
          return;
        }
        setTimeout(run, 0);
      }
      run();
    };

    // Initialize WASM and start tasks
    Module.ccall('picorb_init', 'number', [], []);

    // Collect and create tasks from Ruby script tags
    try {
      const rubyTasks = await collectRubyScripts();
      rubyTasks.forEach(function(task) {
        Module.ccall('picorb_create_task', 'number', ['string'], [task]);
      });
    } catch (error) {
      console.error('Error loading Ruby tasks:', error);
    }

    // Collect and create tasks from MRB data tags
    try {
      const mrbTasks = await collectMrbVMCode();
      mrbTasks.forEach(function(buffer) {
        const ptr = Module._malloc(buffer.byteLength);
        if (ptr === 0) {
          throw new Error('Failed to allocate memory in Wasm heap.');
        }
        try {
          Module.HEAPU8.set(new Uint8Array(buffer), ptr);
          const result = Module.ccall('picorb_create_task_from_mrb', 'number', ['number', 'number'], [ptr, buffer.byteLength]);
          if (result !== 0) {
            console.error('Failed to create task from mrb.');
          }
        } finally {
          Module._free(ptr);
        }
      });
    } catch (error) {
      console.error('Error loading MRB tasks:', error);
    }

    // Also support window.userTasks if present (for backward compatibility)
    if (window.userTasks) {
      window.userTasks.forEach(function(task) {
        Module.ccall('picorb_create_task', 'number', ['string'], [task]);
      });
    }

    // Start PicoRuby execution
    Module.picorubyRun();
  }

  global.initPicoRuby = initPicoRuby;

  await initPicoRuby();
})(typeof window !== 'undefined' ? window : this).catch(console.error);
