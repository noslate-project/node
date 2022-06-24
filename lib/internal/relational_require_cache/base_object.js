'use strict';

const path = require('path');
const { ArrayIsArray } = primordials;
const { RequireCache } = internalBinding('st_require_cache');

class BaseRelationalRequireCacheObject {
  constructor(entries, dumpFilename) {
    if (!entries) entries = [ process.cwd() ];

    this.entries = (ArrayIsArray(entries) ? entries : [ entries ]).map((p) => {
      while (p && p[p.length - 1] === path.sep) {
        p = p.substr(0, p.length - 1);
      }
      return p;
    });
    this.cache = new RequireCache(dumpFilename, this.entries);
  }

  isLoaded() {
    return this.cache.isLoaded();
  }
}

module.exports = BaseRelationalRequireCacheObject;
