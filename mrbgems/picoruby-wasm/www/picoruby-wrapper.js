// picoruby-wrapper.js

async function initPicoRuby() {
  // Import the factory function
  const { default: createModule } = await import('./picoruby.js')

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

  // Create and initialize the module
  const Module = await createModule();

  Module.picorubyRun = function() {
    const MRBC_TICK_UNIT = 4;
    let lastTime = Date.now();
    function run() {
      const currentTime = Date.now();
      if (MRBC_TICK_UNIT <= currentTime - lastTime) {
        Module.ccall('wasm_tick', null, [], []);
        lastTime = currentTime;
      }
      const result = Module.ccall('wasm_run_step', 'number', [], []);
      if (0 <= result) { // No task is running nor waiting
        setTimeout(run, 0);
      }
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

  // Also support window.userTasks if present (for backward compatibility)
  if (window.userTasks) {
    window.userTasks.forEach(function(task) {
      Module.ccall('picoruby_create_task', 'number', ['string'], [task]);
    });
  }

  // Start PicoRuby execution
  Module.picorubyRun();
}

// Start initialization
initPicoRuby().catch(console.error);
