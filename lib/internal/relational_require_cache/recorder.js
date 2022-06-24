'use strict';

/* eslint-disable max-len */

const { Symbol } = primordials;
const { Error } = primordials;

const {
  findLongestRegisteredExtension,
} = require('internal/modules/cjs/loader');

const debug = require('internal/util/debuglog')
  .debuglog('require_cache/recorder');
const fs = require('fs');
const Module = require('module');
const path = require('path');
const vm = require('vm');

const BaseObject = require('internal/relational_require_cache/base_object');

const INJECT = Symbol('RelationalRequireCacheRecorder#inject');
const ORIG_COMPILE = Symbol('RelationalRequireCacheRecorder#origCompile');
const ORIG_DOT_JSON = Symbol('RelationalRequireCacheRecorder#origDotJSON');
const ORIG_RESOLVE_FILENAME = Symbol('RelationalRequireCacheRecorder#origResolveFilename');

class RelationalRequireCacheRecorder extends BaseObject {
  constructor(entries = process.cwd()) {
    if (BaseObject.loader || BaseObject.recorder) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('A alinode process only can create one require cache recorder / loader.');
    }

    super(entries);

    BaseObject.recorder = this;
  }

  start() {
    this[INJECT]();
  }

  dump(filename) {
    return this.cache.dump(filename);
  }

  [INJECT]() {
    const self = this;

    // Module._resolveFilename
    {
      Module[ORIG_RESOLVE_FILENAME] = Module._resolveFilename;
      // eslint-disable-next-line func-name-matching
      Module._resolveFilename = function _resolveFilenameWithRRCW(request, parent, isMain, options) {
        const filename = Module[ORIG_RESOLVE_FILENAME](request, parent, isMain, options);

        // inject logic below
        if (
          path.isAbsolute(filename) &&
          [ '.js', '.json' ].includes(findLongestRegisteredExtension(filename))
        ) {
          const parentFilename = parent ?
            self.cache.transformFilename(parent.filename) :
            '';

          const target = self.cache.transformFilename(filename);
          const transformedRequest = path.isAbsolute(request) ?
            self.cache.transformFilename(request) :
            request;

          debug('addRelation', parentFilename, transformedRequest, target);
          self.cache.addRelation(parentFilename, transformedRequest, target);
        }

        return filename;
      };
    }

    // Module.prototype._compile
    {
      Module.prototype[ORIG_COMPILE] = Module.prototype._compile;
      // eslint-disable-next-line func-name-matching
      Module.prototype._compile = function _compileWithRRCW(content, filename) {
        const wrapped = Module.wrap(content);
        const script = new vm.Script(wrapped, {
          filename,
          lineOffset: 0,
          displayErrors: true,
        });

        const buff = script.createCachedData();

        self.cache.addCode(self.cache.transformFilename(filename), wrapped, buff);

        return this[ORIG_COMPILE](content, filename);
      };
    }

    // Module._extensions['.json']
    {
      Module._extensions[ORIG_DOT_JSON] = Module._extensions['.json'];
      Module._extensions['.json'] = function dotJSONWithRRCW(module, filename) {
        const content = fs.readFileSync(filename, 'utf8');
        self.cache.addCode(self.cache.transformFilename(filename), content, null);
        return Module._extensions[ORIG_DOT_JSON](module, filename);
      };
    }
  }
}

module.exports = RelationalRequireCacheRecorder;
