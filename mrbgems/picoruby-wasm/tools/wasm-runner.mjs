#!/usr/bin/env node
/**
 * wasm-runner.mjs - Node.js wrapper for running PicoRuby WASM
 *
 * Usage: node wasm-runner.mjs <ruby_script.rb>
 *
 * This script loads the PicoRuby WASM module and executes the given Ruby script,
 * making it compatible with the Picotest test runner.
 */

import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, resolve } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// Path to the WASM build output
const WASM_DIR = resolve(__dirname, '../../../build/picoruby-wasm-test/bin');

async function main() {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    console.error('Usage: node wasm-runner.mjs <ruby_script.rb>');
    process.exit(1);
  }

  const scriptPath = args[0];
  let rubyCode;
  try {
    rubyCode = readFileSync(scriptPath, 'utf-8');
  } catch (e) {
    console.error(`Failed to read Ruby script: ${scriptPath}`);
    console.error(e.message);
    process.exit(1);
  }

  // Dynamically import the WASM module
  const wasmModulePath = resolve(WASM_DIR, 'picoruby.js');
  let Module;
  try {
    const createModule = (await import(wasmModulePath)).default;
    Module = await createModule({
      print: (text) => process.stdout.write(text + '\n'),
      printErr: (text) => process.stderr.write(text + '\n'),
    });
  } catch (e) {
    console.error(`Failed to load WASM module from: ${wasmModulePath}`);
    console.error(e.message);
    process.exit(1);
  }

  // Initialize PicoRuby
  const initResult = Module.ccall('picorb_init', 'number', [], []);
  if (initResult !== 0) {
    console.error('Failed to initialize PicoRuby');
    process.exit(1);
  }

  // Create a task with the Ruby code
  const createResult = Module.ccall(
    'picorb_create_task_with_filename',
    'number',
    ['string', 'string'],
    [rubyCode, scriptPath]
  );
  if (createResult !== 0) {
    console.error('Failed to create Ruby task');
    process.exit(1);
  }

  // Run the task until completion
  let exitCode = 0;
  let idleCount = 0;
  const MAX_IDLE = 100;  // Exit after N consecutive idle cycles

  while (true) {
    const result = Module.ccall('mrb_run_step', 'number', [], []);
    if (result < 0) {
      exitCode = 1;
      break;
    }

    // Tick the VM
    Module.ccall('mrb_tick_wasm', null, [], []);

    // Check if we've been idle for a while (no tasks running)
    // This is a heuristic - in real usage, tasks finish when all code executes
    idleCount++;
    if (idleCount > MAX_IDLE) {
      break;
    }

    // Small delay to prevent tight loop
    await new Promise(resolve => setTimeout(resolve, 1));
  }

  process.exit(exitCode);
}

main().catch((e) => {
  console.error('Unexpected error:', e);
  process.exit(1);
});
