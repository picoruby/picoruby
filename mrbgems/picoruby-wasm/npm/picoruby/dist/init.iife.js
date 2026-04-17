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
          const code = await response.text();
          const filename = script.src.split('/').pop() || script.src;
          return { code, filename };
        }
        return { code: script.textContent.trim(), filename: null };
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
      const MRB_TICK_UNIT = 4; // Must match the value in build_config/picoruby-wasm.rb
      const BATCH_DURATION = 16; // ~1 frame (16.67ms)
      const MAX_CATCHUP_TICKS = 10; // Cap to avoid freeze when tab returns from background
      let lastTick = performance.now();
      function run() {
        const now = performance.now();
        let tickCount = 0;
        while (now - lastTick >= MRB_TICK_UNIT && tickCount < MAX_CATCHUP_TICKS) {
          Module._mrb_tick_wasm();
          lastTick += MRB_TICK_UNIT;
          tickCount++;
        }
        if (now - lastTick >= MRB_TICK_UNIT) {
          lastTick = now;
        }
        const sliceStart = performance.now();
        while (performance.now() - sliceStart < BATCH_DURATION) {
          const result = Module._mrb_run_step();
          if (result < 0) {
            console.error('mrb_run_step returned', result, '- scheduler continues');
          }
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
        if (task.filename) {
          Module.ccall('picorb_create_task_with_filename', 'number',
                       ['string', 'string'], [task.code, task.filename]);
        } else {
          Module.ccall('picorb_create_task', 'number', ['string'], [task.code]);
        }
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
        Module.ccall('picorb_create_task', 'number', ['string'],
                     [typeof task === 'string' ? task : task.code]);
      });
    }

    // Start PicoRuby execution
    Module.picorubyRun();
  }

  global.initPicoRuby = initPicoRuby;

  await initPicoRuby();
})(typeof window !== 'undefined' ? window : this).catch(console.error);
