'use strict';

const { Symbol } = primordials;
const { DateNow } = primordials;
const Module = require('module');

const ORIG_REQUIRE = Symbol('Module#origRequire');
const INJECT = Symbol('CostWatcher#inject');

class CostWatcher {
  constructor() {
    this.open = false;
    this.time = 0;
  }

  start() {
    this[INJECT]();
  }

  stop() {
    if (!Module.prototype[ORIG_REQUIRE]) return;

    Module.prototype.require = Module.prototype[ORIG_REQUIRE];
    Module.prototype[ORIG_REQUIRE] = undefined;
  }

  get cost() {
    return this.time;
  }

  [INJECT]() {
    if (Module.prototype[ORIG_REQUIRE]) return;

    const self = this;
    Module.prototype[ORIG_REQUIRE] = Module.prototype.require;
    Module.prototype.require = function(id) {
      let now;
      if (!self.open) {
        self.open = true;
        now = DateNow();
      }

      const ret = this[ORIG_REQUIRE](id);

      if (now) {
        self.open = false;
        self.time += (DateNow() - now);
      }

      return ret;
    };
  }
}

module.exports = CostWatcher;
