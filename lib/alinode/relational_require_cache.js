'use strict';

/* eslint-disable max-len */


const { ArrayIsArray } = primordials;
const assert = require('assert');

const CostWatcher = require('internal/relational_require_cache/cost_watcher');
const Loader = require('internal/relational_require_cache/loader');
const Recorder = require('internal/relational_require_cache/recorder');
const requireCache = require('internal/relational_require_cache/require_cache');

let costWatcher = null;
let loader = null;
let recorder = null;

function record(entries = process.cwd()) {
  assert(!loader, 'Strontium should not run in Load Require Cache mode');
  assert(!recorder, 'Record called');
  assert(typeof entries === 'string' || ArrayIsArray(entries), '`entries` should be string or array.');

  recorder = new Recorder(entries);
  recorder.start();
}

function dump(filename) {
  assert(recorder && recorder.isLoaded(), 'Not recorded yet');
  assert(typeof filename === 'string', 'Filename should be a string');
  recorder.dump(filename);
}

function load(filename, entries = process.cwd(), options = undefined) {
  assert(!loader, 'Load called');
  assert(!recorder, 'Strontium should not run in Record Require Cache mode');
  assert(typeof entries === 'string' || ArrayIsArray(entries), '`entries` should be string or array.');
  assert(typeof filename === 'string', 'Filename should be a string');

  loader = new Loader(filename, entries, options);
  loader.start();
}

function trackCost() {
  if (!costWatcher) {
    costWatcher = new CostWatcher();
  }

  costWatcher.start();
}

function stopTrackCost() {
  if (!costWatcher) return 0;
  costWatcher.stop();
  return costWatcher.cost;
}

module.exports = {
  record,
  dump,
  load,
  trackCost,
  stopTrackCost,

  RequireCacheReader: requireCache.Reader,
  RequireCacheWriter: requireCache.Writer,
};
