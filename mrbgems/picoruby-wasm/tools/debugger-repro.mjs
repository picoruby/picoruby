#!/usr/bin/env node
/**
 * debugger-repro.mjs - Drive the PicoRuby debugger panel's calls under Node.
 *
 * Loads the SAME PicoRuby.wasm debug build a Funicular page uses (from the
 * Rails app's public/picoruby/debug/), logs in to the running Rails dev
 * server, fetches the real page HTML, loads the real compiled app.mrb the
 * way init.iife.js does, and lets Funicular.start run against the real API.
 * Then it drives two things at once, exactly like the browser:
 *
 *   1. the scheduler loop from init.iife.js (setTimeout(run, 0|4))
 *   2. the calls the debugger extension's panel.js performs:
 *        - mrb_debug_get_status              every 200ms
 *        - mrb_get_component_debug_info      every 500ms (+ tree fetch on change)
 *        - mrb_eval_string (debug-mode probe) once
 *        - mrb_get_component_state_by_id     when --inspect-component is given
 *          (what clicking a component in the COMPONENTS pane does)
 *
 * It prints one stats line per second and diagnoses:
 *   - HANG: a single ccall never returns (a watchdog thread reports the op
 *           and kills the process)
 *   - SPIN: run() is rescheduled with delay 0 although nothing progressed
 *   - Ruby exceptions and wasm aborts, as printed by the runtime
 *
 * Only IndexedDB and Web Locks are missing from a real browser; pass
 * --fake-idb to get a working IndexedDB, otherwise the local database runs
 * volatile.
 *
 * Usage:
 *   node debugger-repro.mjs --app <dir> --live <url> --user <name> --password <pw> [options]
 *     --app <dir>          Funicular Rails app root (its node_modules must have jsdom)
 *     --live <url>         page to load, e.g. http://localhost:3000/chat
 *     --user <name>        login user (POST /login)
 *     --password <pw>      login password
 *     --runtime <dir>      dir with picoruby.js/.wasm (default: <app>/public/picoruby/debug)
 *     --seconds <n>        wall-clock run time (default: 15)
 *     --no-debugger        run the app alone (baseline)
 *     --no-status-poll     skip the 200ms mrb_debug_get_status poll
 *     --no-tree-poll       skip the 500ms component_tree poll
 *     --inspect-component <ids>  comma list of component ids to "click"
 *     --fake-idb <dir>     dir whose node_modules has fake-indexeddb
 *     --no-page-scripts    do not run the page's own classic scripts
 *                          (funicular_debug.js highlighter) in jsdom
 *     --debugger-delay <ms> open the "panel" this long after start instead of
 *                          waiting for the first mount
 *     --probe <ruby>       evaluate this Ruby expression through
 *                          mrb_eval_string periodically and print the result
 *     --probe-ms <n>       probe interval (default: 5000)
 *     --gc-stress-ms <n>   force GC.start inside a synchronous eval this often
 *     --hang-ms <n>        watchdog threshold (default: 5000)
 *     --trace              print every debugger ccall with its duration
 */

import { readFileSync, writeSync } from 'node:fs';
import { createRequire } from 'node:module';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { Worker, isMainThread, workerData } from 'node:worker_threads';

// ---------------------------------------------------------------------------
// Watchdog worker: runs on its own thread, so it keeps ticking while the main
// thread is stuck inside wasm. It writes with fs.writeSync (worker stdio is
// otherwise proxied through the blocked main thread) and then kills the
// process.
// ---------------------------------------------------------------------------
if (!isMainThread) {
  const { sab, opNames, hangMs } = workerData;
  const hb = new Float64Array(sab, 0, 1);
  const op = new Int32Array(sab, 8, 1);
  setInterval(() => {
    const last = hb[0];
    if (last === 0) return;
    const silent = Date.now() - last;
    if (silent > hangMs) {
      const name = opNames[Atomics.load(op, 0)] || '?';
      writeSync(2, `\n*** HANG: no heartbeat for ${silent}ms; last op = ${name} ***\n`);
      writeSync(2, `*** The main thread is stuck inside that ccall. ***\n`);
      process.kill(process.pid, 'SIGKILL');
    }
  }, 250);
} else {
  main().catch((e) => {
    console.error(e.stack || String(e));
    process.exit(1);
  });
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------
function usage(message) {
  console.error(message);
  console.error('usage: node debugger-repro.mjs --app <dir> --live <url> --user <name> --password <pw> [options]');
  process.exit(1);
}

function parseArgs(argv) {
  const opts = {
    app: null,
    live: null,
    user: null,
    password: null,
    runtime: null,
    seconds: 15,
    debugger: true,
    statusPoll: true,
    treePoll: true,
    inspectComponent: null,
    fakeIdb: null,
    pageScripts: true,
    debuggerDelay: null,
    probe: null,
    probeMs: 5000,
    gcStressMs: 0,
    hangMs: 5000,
    trace: false,
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    switch (a) {
      case '--app': opts.app = resolve(argv[++i]); break;
      case '--live': opts.live = argv[++i]; break;
      case '--user': opts.user = argv[++i]; break;
      case '--password': opts.password = argv[++i]; break;
      case '--runtime': opts.runtime = resolve(argv[++i]); break;
      case '--seconds': opts.seconds = Number(argv[++i]); break;
      case '--no-debugger': opts.debugger = false; break;
      case '--no-status-poll': opts.statusPoll = false; break;
      case '--no-tree-poll': opts.treePoll = false; break;
      case '--inspect-component': opts.inspectComponent = argv[++i]; break;
      case '--fake-idb': opts.fakeIdb = resolve(argv[++i]); break;
      case '--no-page-scripts': opts.pageScripts = false; break;
      case '--debugger-delay': opts.debuggerDelay = Number(argv[++i]); break;
      case '--probe': opts.probe = argv[++i]; break;
      case '--probe-ms': opts.probeMs = Number(argv[++i]); break;
      case '--gc-stress-ms': opts.gcStressMs = Number(argv[++i]); break;
      case '--hang-ms': opts.hangMs = Number(argv[++i]); break;
      case '--trace': opts.trace = true; break;
      default: usage(`Unknown option: ${a}`);
    }
  }
  for (const key of ['app', 'live', 'user', 'password']) {
    if (!opts[key]) usage(`--${key} is required`);
  }
  if (!opts.runtime) opts.runtime = join(opts.app, 'public', 'picoruby', 'debug');
  return opts;
}

// ---------------------------------------------------------------------------
// jsdom (same setup as funicular/lib/funicular/testing/node_runner.mjs)
// ---------------------------------------------------------------------------
function installDom(appRoot, html, url) {
  const appRequire = createRequire(join(appRoot, 'package.json'));
  const { JSDOM } = appRequire('jsdom');
  const dom = new JSDOM(html, {
    url,
    pretendToBeVisual: true,
    runScripts: 'outside-only',
  });
  const win = dom.window;
  globalThis.window = win;
  globalThis.document = win.document;
  globalThis.location = win.location;
  globalThis.history = win.history;
  Object.defineProperty(globalThis, 'navigator', { value: win.navigator, configurable: true });
  Object.defineProperty(globalThis, 'localStorage', { value: win.localStorage, configurable: true });
  globalThis.Event = win.Event;
  globalThis.CustomEvent = win.CustomEvent;
  globalThis.MouseEvent = win.MouseEvent;
  globalThis.KeyboardEvent = win.KeyboardEvent;
  globalThis.InputEvent = win.InputEvent;
  globalThis.FormData = win.FormData;
  globalThis.Element = win.Element;
  globalThis.Document = win.Document;
  globalThis.HTMLElement = win.HTMLElement;
  globalThis.Node = win.Node;
  globalThis.Text = win.Text;
  globalThis.Response = globalThis.Response || win.Response;
  globalThis.fetch = globalThis.fetch || win.fetch?.bind(win);
  win.fetch = globalThis.fetch;

  // JS.global is bound to `window` (js.c: rootObject = window). In a browser
  // window === globalThis, but here window.eval is jsdom's realm: a Promise
  // it creates fails the wasm side's `instanceof Promise` (Node realm), so
  // every `JS.global.eval(...).await` (Web Locks shim, storage.persist,
  // await_inflight_persists) raises "Method not found: await". Keep the
  // evaluation in jsdom's realm (its globals must land on window) but hand
  // back a Node-realm Promise that adopts a jsdom thenable.
  const jsdomEval = win.eval;
  Object.defineProperty(win, 'eval', {
    value: (code) => {
      const r = jsdomEval(code);
      if (r && typeof r.then === 'function' && !(r instanceof Promise)) return Promise.resolve(r);
      return r;
    },
    configurable: true, writable: true,
  });
  return { dom, jsdomEval };
}

// Minimal Web Locks API: exclusive/shared, ifAvailable, FIFO queue.
function makeLockManager() {
  const held = new Map();
  const queues = new Map();
  const pump = (name) => {
    const q = queues.get(name);
    if (q && q.length) q.shift()();
  };
  const request = (name, options, callback) => {
    if (typeof options === 'function') { callback = options; options = {}; }
    options = options || {};
    const mode = options.mode || 'exclusive';
    return new Promise((resolve, reject) => {
      const attempt = () => {
        const cur = held.get(name);
        const free = !cur || (mode === 'shared' && cur.mode === 'shared');
        if (free) {
          held.set(name, { mode, count: (cur?.count || 0) + 1 });
          const release = () => {
            const h = held.get(name);
            if (!h) return;
            if (h.count <= 1) held.delete(name); else h.count--;
            pump(name);
          };
          let result;
          try { result = callback({ name, mode }); } catch (e) { release(); reject(e); return; }
          Promise.resolve(result).then((v) => { release(); resolve(v); },
                                       (e) => { release(); reject(e); });
        } else if (options.ifAvailable) {
          Promise.resolve(callback(null)).then(resolve, reject);
        } else {
          if (!queues.has(name)) queues.set(name, []);
          queues.get(name).push(attempt);
        }
      };
      attempt();
    });
  };
  const query = async () => ({
    held: [...held].map(([name, h]) => ({ name, mode: h.mode })),
    pending: [...queues].filter(([, q]) => q.length).map(([name]) => ({ name })),
  });
  return { request, query };
}

// Give the page the browser APIs the persistent local database needs:
// IndexedDB (fake-indexeddb), navigator.locks, navigator.storage.persist().
function installBrowserApiPolyfills(win, appRoot, fakeIdbDir) {
  const nav = win.navigator;
  Object.defineProperty(nav, 'locks', { value: makeLockManager(), configurable: true });
  Object.defineProperty(nav, 'storage', {
    value: { persist: async () => true, persisted: async () => true, estimate: async () => ({}) },
    configurable: true,
  });
  const candidates = [fakeIdbDir, appRoot].filter(Boolean);
  for (const dir of candidates) {
    try {
      const req = createRequire(join(dir, 'package.json'));
      const { indexedDB, IDBKeyRange } = req('fake-indexeddb');
      globalThis.indexedDB = indexedDB;
      globalThis.IDBKeyRange = IDBKeyRange;
      win.indexedDB = indexedDB;
      win.IDBKeyRange = IDBKeyRange;
      console.log(`[repro] IndexedDB: fake-indexeddb from ${dir}`);
      return true;
    } catch (e) {
      if (!/Cannot find module/.test(e.message)) throw e;
    }
  }
  console.log('[repro] IndexedDB: unavailable (npm i fake-indexeddb somewhere and pass --fake-idb <dir>); DB runs volatile');
  return false;
}

// Run the page's own classic scripts (e.g. funicular_debug.js, the
// MutationObserver-based component highlighter) inside jsdom, skipping the
// PicoRuby loader which this harness replaces.
async function runPageScripts(shim, pageUrl, jsdomEval) {
  const doc = globalThis.document;
  for (const script of doc.querySelectorAll('script')) {
    const type = (script.getAttribute('type') || '').trim();
    if (type && type !== 'text/javascript' && type !== 'module') continue;
    const src = script.getAttribute('src');
    let code;
    if (src) {
      if (/init\.iife\.js/.test(src)) continue;
      const res = await shim(new URL(src, pageUrl).href);
      if (!res.ok) { console.error(`[repro] GET ${src} -> ${res.status}`); continue; }
      code = await res.text();
    } else {
      code = script.textContent;
    }
    try {
      jsdomEval(code);
      console.log(`[repro] ran page script ${src || '(inline)'}`);
    } catch (e) {
      console.error(`[repro] page script ${src || '(inline)'} threw: ${e.message}`);
    }
  }
}

// An emscripten ENVIRONMENT=web build refuses to run when it sees Node's
// `process`: the factory throws on `typeof process !== 'undefined'` and
// asserts !ENVIRONMENT_IS_NODE, both evaluated once when createModule()
// starts. Hide `process` while the factory runs and restore it afterwards.
async function withoutProcess(fn) {
  const desc = Object.getOwnPropertyDescriptor(globalThis, 'process');
  Object.defineProperty(globalThis, 'process', { value: undefined, configurable: true, writable: true });
  try {
    return await fn();
  } finally {
    Object.defineProperty(globalThis, 'process', desc);
  }
}

// fetch shim:
//   - files under the runtime dir are served from disk (the web build fetches
//     picoruby.wasm; Node's fetch cannot read file paths)
//   - relative URLs ("/current_user") resolve against the page origin, the
//     way the browser does for Funicular::Http
//   - a minimal cookie jar keeps the Rails session across requests
function makeFetchShim(runtimeDir, pageOrigin) {
  const realFetch = globalThis.fetch;
  const jar = new Map();
  const cookieHeader = () => [...jar].map(([k, v]) => `${k}=${v}`).join('; ');
  const shim = async (input, init = {}) => {
    let url = typeof input === 'string' ? input : input?.url;
    if (typeof url === 'string') {
      if (url.startsWith('file://')) url = new URL(url).pathname;
      if (url.startsWith(runtimeDir)) {
        const body = readFileSync(url);
        const type = url.endsWith('.wasm') ? 'application/wasm' : 'application/octet-stream';
        return new Response(body, { status: 200, headers: { 'Content-Type': type } });
      }
      if (pageOrigin && !/^[a-z]+:/i.test(url)) url = new URL(url, pageOrigin).href;
    }
    const sameOrigin = pageOrigin && typeof url === 'string' && url.startsWith(pageOrigin);
    if (sameOrigin && jar.size) {
      const headers = new Headers(init.headers || (typeof input !== 'string' ? input.headers : undefined));
      headers.set('Cookie', cookieHeader());
      init = { ...init, headers };
    }
    const res = await realFetch(url, init);
    if (sameOrigin) {
      for (const sc of res.headers.getSetCookie?.() || []) {
        const [pair] = sc.split(';');
        const eq = pair.indexOf('=');
        if (eq > 0) jar.set(pair.slice(0, eq).trim(), pair.slice(eq + 1).trim());
      }
    }
    return res;
  };
  shim.cookieHeader = cookieHeader;
  return shim;
}

function installFetchShim(shim) {
  globalThis.fetch = shim;
  if (globalThis.window) globalThis.window.fetch = shim;
}

// Browsers resolve `new WebSocket("/cable")` against the page; Node's
// WebSocket wants an absolute ws:// URL and sends no cookies. Use the `ws`
// package (a jsdom dependency) so the Rails session cookie reaches
// ActionCable's connection auth.
function installWebSocketShim(appRoot, pageOrigin, cookieHeader) {
  const appRequire = createRequire(join(appRoot, 'package.json'));
  const WS = appRequire('ws');
  const resolveUrl = (url) => {
    const abs = new URL(String(url), pageOrigin);
    if (abs.protocol === 'http:') abs.protocol = 'ws:';
    if (abs.protocol === 'https:') abs.protocol = 'wss:';
    return abs.href;
  };
  class PageWebSocket extends WS {
    constructor(url, protocols) {
      const headers = {};
      const cookie = cookieHeader();
      if (cookie) headers.Cookie = cookie;
      headers.Origin = pageOrigin;
      super(resolveUrl(url), protocols, { headers });
    }
  }
  globalThis.WebSocket = PageWebSocket;
  if (globalThis.window) globalThis.window.WebSocket = PageWebSocket;
}

// Log in, then fetch the page HTML the browser would get.
async function fetchLivePage(shim, opts) {
  const origin = new URL(opts.live).origin;
  const login = await shim(new URL('/login', origin).href, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', 'Accept': 'application/json' },
    body: JSON.stringify({ username: opts.user, password: opts.password }),
  });
  if (!login.ok) throw new Error(`login failed: ${login.status} ${await login.text()}`);
  console.log(`[repro] logged in as ${opts.user}`);
  // A catch-all HTML route only answers HTML-formatted requests.
  const page = await shim(opts.live, { headers: { 'Accept': 'text/html' } });
  if (!page.ok) throw new Error(`GET ${opts.live} failed: ${page.status}`);
  return page.text();
}

// What init.iife.js does after Module creation: collect
// <script type="text/ruby"> and <script type="application/x-mrb"> tags.
async function createLiveTasks(Module, shim, pageUrl) {
  const doc = globalThis.document;
  let count = 0;
  for (const script of doc.querySelectorAll('script[type="text/ruby"]')) {
    let code, filename = null;
    if (script.src) {
      const res = await shim(new URL(script.src, pageUrl).href);
      code = await res.text();
      filename = script.src.split('/').pop();
    } else {
      code = script.textContent.trim();
    }
    const r = filename
      ? Module.ccall('picorb_create_task_with_filename', 'number', ['string', 'string'], [code, filename])
      : Module.ccall('picorb_create_task', 'number', ['string'], [code]);
    if (r !== 0) throw new Error('picorb_create_task failed');
    count++;
  }
  for (const script of doc.querySelectorAll('script[type="application/x-mrb"]')) {
    if (!script.src) continue;
    const src = new URL(script.getAttribute('src'), pageUrl).href;
    const res = await shim(src);
    if (!res.ok) throw new Error(`GET ${src} failed: ${res.status}`);
    const buffer = await res.arrayBuffer();
    const ptr = Module._malloc(buffer.byteLength);
    try {
      Module.HEAPU8.set(new Uint8Array(buffer), ptr);
      const r = Module.ccall('picorb_create_task_from_mrb', 'number', ['number', 'number'], [ptr, buffer.byteLength]);
      if (r !== 0) throw new Error('picorb_create_task_from_mrb failed');
    } finally {
      Module._free(ptr);
    }
    console.log(`[repro] loaded ${src} (${buffer.byteLength} bytes)`);
    count++;
  }
  if (count === 0) throw new Error('no Ruby/mrb script tags found in the page');
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
async function main() {
  const opts = parseArgs(process.argv.slice(2));
  console.log(`[repro] app=${opts.app}`);
  console.log(`[repro] runtime=${opts.runtime}`);
  console.log(`[repro] debugger=${opts.debugger} statusPoll=${opts.statusPoll} treePoll=${opts.treePoll}`);

  // Heartbeat shared with the watchdog thread
  const OPS = ['idle', 'run:tick', 'run:step', 'run:gcPending', 'poll:status',
               'poll:tree_check', 'poll:tree_fetch', 'probe:eval', 'init'];
  const sab = new SharedArrayBuffer(16);
  const hb = new Float64Array(sab, 0, 1);
  const opCell = new Int32Array(sab, 8, 1);
  const beat = (opIndex) => { Atomics.store(opCell, 0, opIndex); hb[0] = Date.now(); };
  new Worker(new URL(import.meta.url), {
    workerData: { sab, opNames: OPS, hangMs: opts.hangMs },
  }).unref();

  const pageOrigin = new URL(opts.live).origin;
  const shim = makeFetchShim(opts.runtime, pageOrigin);
  const html = await fetchLivePage(shim, opts);
  const pageUrl = opts.live;
  const { jsdomEval } = installDom(opts.app, html, pageUrl);
  installFetchShim(shim);
  installWebSocketShim(opts.app, pageOrigin, shim.cookieHeader);
  installBrowserApiPolyfills(globalThis.window, opts.app, opts.fakeIdb);
  if (opts.pageScripts) await runPageScripts(shim, pageUrl, jsdomEval);
  const { default: createModule } = await import(pathToFileURL(join(opts.runtime, 'picoruby.js')).href);
  const stdout = process.stdout, stderr = process.stderr;
  const Module = await withoutProcess(() => createModule({
    locateFile: (file) => join(opts.runtime, file),
    print: (t) => stdout.write(t + '\n'),
    printErr: (t) => stderr.write(t + '\n'),
  }));
  globalThis.window.picorubyModule = Module;

  beat(8);
  if (Module.ccall('picorb_init', 'number', [], []) !== 0) throw new Error('picorb_init failed');

  await createLiveTasks(Module, shim, pageUrl);

  for (const name of ['mrb_run_step_status', 'mrb_gc_scheduler_pending_wasm', 'mrb_tick_wasm',
                      'mrb_debug_get_status', 'mrb_get_component_debug_info',
                      'mrb_get_component_state_by_id', 'mrb_eval_string']) {
    if (typeof Module['_' + name] !== 'function') {
      throw new Error(`export missing in this build: ${name}`);
    }
  }

  // ---- stats -------------------------------------------------------------
  const stats = {
    runs: 0, delay0: 0, delay0NoProgress: 0, steps: 0, maxRunMs: 0,
    pendingAtEnd: 0, exceptions: 0,
    status: { n: 0, maxMs: 0, totalMs: 0 },
    treeCheck: { n: 0, maxMs: 0, totalMs: 0, bytes: 0, changes: 0 },
    treeFetch: { n: 0, maxMs: 0, totalMs: 0, components: 0 },
  };
  const slow = [];
  const timed = (bucket, opIndex, label, fn) => {
    beat(opIndex);
    const t0 = performance.now();
    const r = fn();
    const ms = performance.now() - t0;
    beat(0);
    bucket.n++;
    bucket.totalMs += ms;
    bucket.winN = (bucket.winN || 0) + 1;
    bucket.winMs = (bucket.winMs || 0) + ms;
    if (ms > bucket.maxMs) bucket.maxMs = ms;
    if (ms > 50) slow.push(`${label} ${ms.toFixed(0)}ms`);
    if (opts.trace) console.log(`[trace] ${label} ${ms.toFixed(1)}ms`);
    return r;
  };

  // ---- scheduler loop: verbatim logic from init.iife.js -----------------
  const MRB_TICK_UNIT = 4;
  const BATCH_DURATION = 16;
  const IDLE_DELAY = 4;
  const MAX_CATCHUP_TICKS = 10;
  let lastTick = performance.now();
  let stop = false;
  const runStepStatus = () => Module.ccall('mrb_run_step_status', 'number', [], []);
  const gcSchedulerPending = () => Module.ccall('mrb_gc_scheduler_pending_wasm', 'number', [], []);

  function run() {
    if (stop) return;
    const runStart = performance.now();
    const now = runStart;
    let tickCount = 0;
    beat(1);
    while (now - lastTick >= MRB_TICK_UNIT && tickCount < MAX_CATCHUP_TICKS) {
      Module._mrb_tick_wasm();
      lastTick += MRB_TICK_UNIT;
      tickCount++;
    }
    if (now - lastTick >= MRB_TICK_UNIT) lastTick = now;
    const sliceStart = performance.now();
    let progressed = false;
    while (performance.now() - sliceStart < BATCH_DURATION) {
      beat(2);
      const status = runStepStatus();
      stats.steps++;
      if (status < 0) {
        stats.exceptions++;
        console.error('mrb_run_step_status returned', status, '- scheduler continues');
        break;
      }
      if (status === 0) break;
      progressed = true;
    }
    beat(3);
    const pending = gcSchedulerPending() === 1;
    beat(0);
    const delay = progressed || pending ? 0 : IDLE_DELAY;
    stats.runs++;
    if (pending) stats.pendingAtEnd++;
    if (delay === 0) {
      stats.delay0++;
      if (!progressed) stats.delay0NoProgress++;
    }
    const runMs = performance.now() - runStart;
    if (runMs > stats.maxRunMs) stats.maxRunMs = runMs;
    setTimeout(run, delay);
  }
  run();

  // ---- debugger panel emulation (panel.js) --------------------------------
  let lastHash = null;
  const simpleHash = (str) => {
    let h = 0;
    for (let i = 0; i < str.length; i++) { h = ((h << 5) - h) + str.charCodeAt(i); h = h & h; }
    return h;
  };
  const timers = [];
  const startDebugger = () => {
    // checkComponentDebugMode()
    const probe = timed(stats.status, 7, 'probe:eval', () => Module.ccall(
      'mrb_eval_string', 'string', ['string'],
      ["global_variables.include?(:$__funicular_debug__) ? 'enabled' : 'disabled'"]));
    console.log(`[repro] debug-mode probe -> ${probe}`);

    if (opts.statusPoll) {
      timers.push(setInterval(() => {
        const s = timed(stats.status, 4, 'poll:status',
                        () => Module.ccall('mrb_debug_get_status', 'string', [], []));
        try { JSON.parse(s); } catch (e) { console.error('[repro] status not JSON:', s); }
      }, 200));
    }
    if (opts.inspectComponent) {
      // What the panel does when a component is clicked: inspectComponent()
      // -> mrb_get_component_state_by_id, which inspects every state value
      // and instance variable of that component. A comma list clicks them
      // one after another.
      const ids = String(opts.inspectComponent).split(',').map(Number).filter((n) => n > 0);
      ids.forEach((id, i) => {
        timers.push(setTimeout(() => {
          console.log(`[repro] inspecting component #${id} (mrb_get_component_state_by_id)`);
          const s = timed(stats.status, 7, 'panel:inspect', () => Module.ccall(
            'mrb_get_component_state_by_id', 'string', ['number'], [id]));
          console.log(`[repro] component #${id} state -> ${s.slice(0, 160)}`);
        }, 1500 + i * 700));
      });
    }
    if (opts.gcStressMs > 0) {
      // Force a full GC from inside a synchronous eval while other tasks
      // are preempted: exercises the task-stack marking under the debugger.
      timers.push(setInterval(() => {
        timed(stats.status, 7, 'gc:stress', () => Module.ccall(
          'mrb_eval_string', 'string', ['string'], ['GC.start; nil']));
      }, opts.gcStressMs));
    }
    if (opts.treePoll) {
      timers.push(setInterval(() => {
        const s = timed(stats.treeCheck, 5, 'poll:tree_check',
                        () => Module.ccall('mrb_get_component_debug_info', 'string',
                                           ['string'], ['component_tree']));
        stats.treeCheck.bytes = s.length;
        const h = simpleHash(s);
        if (lastHash !== null && lastHash !== h) {
          stats.treeCheck.changes++;
          const t = timed(stats.treeFetch, 6, 'poll:tree_fetch',
                          () => Module.ccall('mrb_get_component_debug_info', 'string',
                                             ['string'], ['component_tree']));
          try {
            const parsed = JSON.parse(t);
            const comps = typeof parsed.result === 'string' ? JSON.parse(parsed.result) : parsed.result;
            stats.treeFetch.components = Array.isArray(comps) ? comps.length : -1;
            if (parsed.error) console.error('[repro] component_tree error:', parsed.error);
          } catch (e) {
            console.error('[repro] component_tree not JSON:', t.slice(0, 200));
          }
        }
        lastHash = h;
      }, 500));
    }
  };

  // Start the debugger once something is mounted into #app (as if the user
  // opened the panel after the page loaded), or after 15s if mount never
  // completes; --debugger-delay overrides both with a fixed delay.
  const appHasChildren = () => (globalThis.document.getElementById('app')?.children.length || 0) > 0;
  const startAt = performance.now();
  const mountWait = setInterval(() => {
    const mounted = appHasChildren();
    const elapsed = performance.now() - startAt;
    const due = opts.debuggerDelay !== null ? elapsed >= opts.debuggerDelay
                                            : (mounted || elapsed > 15000);
    if (due) {
      clearInterval(mountWait);
      console.log(`[repro] mounted=${mounted}; starting debugger polls`);
      if (opts.debugger) startDebugger();
    }
  }, 50);

  // ---- per-second report ---------------------------------------------------
  let lastRuns = 0, lastSteps = 0, lastD0 = 0, lastD0NP = 0, lastPending = 0;
  let spinSeconds = 0;
  // avg is over the last second only, so a trend is visible; max is global.
  const fmt = (b) => {
    const avg = b.winN ? (b.winMs / b.winN).toFixed(1) : '-';
    b.winN = 0; b.winMs = 0;
    return `n=${b.n} max=${b.maxMs.toFixed(1)}ms avg1s=${avg}ms`;
  };
  if (opts.probe) {
    timers.push(setInterval(() => {
      const r = timed(stats.status, 7, 'probe', () => Module.ccall(
        'mrb_eval_string', 'string', ['string'], [opts.probe]));
      console.log(`[probe] ${r}`);
    }, opts.probeMs));
  }
  const report = setInterval(() => {
    const runs = stats.runs - lastRuns, steps = stats.steps - lastSteps;
    const d0 = stats.delay0 - lastD0, d0np = stats.delay0NoProgress - lastD0NP;
    const pend = stats.pendingAtEnd - lastPending;
    lastRuns = stats.runs; lastSteps = stats.steps; lastD0 = stats.delay0;
    lastD0NP = stats.delay0NoProgress; lastPending = stats.pendingAtEnd;
    console.log(
      `[stats] run/s=${runs} steps/s=${steps} delay0=${d0} (noProgress=${d0np}) ` +
      `gcPending=${pend} maxRun=${stats.maxRunMs.toFixed(1)}ms exc=${stats.exceptions} | ` +
      `status ${fmt(stats.status)} | tree ${fmt(stats.treeCheck)} bytes=${stats.treeCheck.bytes} ` +
      `changes=${stats.treeCheck.changes} comps=${stats.treeFetch.components}`);
    if (slow.length) { console.log(`[slow] ${slow.join(', ')}`); slow.length = 0; }
    stats.maxRunMs = 0;
    // A healthy idle loop runs ~250/s (4ms delay). Sustained delay-0
    // rescheduling without progress means gcPending is stuck: the browser
    // would peg one core exactly like this.
    if (runs > 400 && d0np > runs * 0.9) {
      spinSeconds++;
      console.log(`[spin] delay-0 reschedule without progress for ${spinSeconds}s (gcPending stuck?)`);
    } else {
      spinSeconds = 0;
    }
  }, 1000);

  await new Promise((r) => setTimeout(r, opts.seconds * 1000));
  stop = true;
  clearInterval(report);
  timers.forEach(clearInterval);
  console.log('[repro] done: no hang detected within the run');
  process.exit(spinSeconds >= 3 ? 3 : 0);
}
