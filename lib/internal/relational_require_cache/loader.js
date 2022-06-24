'use strict';

/* eslint-disable max-len */

const { Symbol } = primordials;
const { Error } = primordials;
const { JSONParse } = primordials;

const Module = require('module');
const path = require('path');
const vm = require('vm');

const debug = require('internal/util/debuglog')
  .debuglog('require_cache/loader');
const { makeRequireFunction } = require('internal/modules/cjs/helpers');

const BaseObject = require('internal/relational_require_cache/base_object');

const INJECT = Symbol('RelationalRequireCacheLoader#inject');
const ORIG_DOT_JS = Symbol('RelationalRequireCacheLoader#origDotJS');
const ORIG_DOT_JSON = Symbol('RelationalRequireCacheLoader#origDotJSON');
const ORIG_RESOLVE_FILENAME = Symbol('RelationalRequireCacheLoader#origResolveFilename');

const { builtinModules } = Module;

class RelationalRequireCacheLoader extends BaseObject {
  constructor(filename, entries = process.cwd(), options = {
    strictMode: false,
  }) {
    if (BaseObject.loader || BaseObject.recorder) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('A alinode process only can create one require cache recorder / loader.');
    }

    super(entries, filename);

    this.strictMode = !!options.strictMode;
    this.started = false;

    BaseObject.loader = this;
  }

  start() {
    if (this.started) return;
    this[INJECT]();
  }

  stop() {
    if (!this.started) return;

    Module._resolveFilename = Module[ORIG_RESOLVE_FILENAME];
    Module[ORIG_RESOLVE_FILENAME] = undefined;

    Module._extensions['.js'] = Module._extensions[ORIG_DOT_JS];
    Module[ORIG_DOT_JS] = undefined;

    Module._extensions['.json'] = Module._extensions[ORIG_DOT_JSON];
    Module[ORIG_DOT_JSON] = undefined;

    this.started = false;
  }

  setStrictMode(strict) {
    this.strictMode = !!strict;
  }

  [INJECT]() {
    const self = this;

    // Module._resolveFilename
    {
      Module[ORIG_RESOLVE_FILENAME] = Module._resolveFilename;
      // eslint-disable-next-line func-name-matching
      Module._resolveFilename = function _resolveFilenameWithRRCR(request, parent, isMain, options) {
        const parentFilename = parent ? self.cache.transformFilename(parent.filename) : '';
        const transformedRequest = path.isAbsolute(request) ?
          self.cache.transformFilename(request) :
          request;
        const cachedPath = self.cache.queryRelation(parentFilename, transformedRequest);

        debug('getCachedPath', parentFilename, transformedRequest, cachedPath);
        if (cachedPath) {
          return self.cache.transformFilenameBack(cachedPath);
        }

        if (self.strictMode && !builtinModules.includes(request)) {
          // eslint-disable-next-line no-restricted-syntax
          const err = new Error(`Cannot find module ${request}`);
          err.code = 'MODULE_NOT_FOUND';
          throw err;
        }

        return Module[ORIG_RESOLVE_FILENAME](request, parent, isMain, options);
      };
    }

    // Module._extensions['.js']
    {
      Module._extensions[ORIG_DOT_JS] = Module._extensions['.js'];
      Module._extensions['.js'] = function dotJSWithRRCR(module, filename) {
        const dummyFilename = self.cache.transformFilename(filename);
        const code = self.cache.queryCode(dummyFilename);
        debug('query code', dummyFilename, code && true);
        if (!code) {
          return Module._extensions[ORIG_DOT_JS](module, filename);
        }

        // try {
        debug('loadCachedJS', filename);
        const script = new vm.Script(code.sourcecode, {
          filename,
          lineOffset: 0,
          displayErrors: true,
          cachedData: code.bytecode,
        });
        // } catch (e) {
        //   throw e;
        // }

        debug('cachedJSBytecode rejected:', script.cachedDataRejected);
        const compiledWrapper = script.runInThisContext({
          displayErrors: true,
        });

        const dirname = path.dirname(filename);
        const require = makeRequireFunction(module /** TODO(kaidi.zkd): policy */);
        const exports = module.exports;
        const thisValue = exports;

        // TODO(kaidi.zkd): inspectorWrapper
        compiledWrapper.call(thisValue, exports, require, module, filename, dirname);
      };
    }

    // Module._extensions['.json']
    {
      Module._extensions[ORIG_DOT_JSON] = Module._extensions['.json'];
      Module._extensions['.json'] = function dotJSONWithRRCR(module, filename) {
        const dummyFilename = self.cache.transformFilename(filename);
        let code = self.cache.queryCode(dummyFilename);
        if (!code) {
          return Module._extensions[ORIG_DOT_JSON](module, filename);
        }

        try {
          debug('loadCachedJSON');
          code = code.sourcecode.toString();
          if (code.charCodeAt(0) === '0xfeff') {
            code = code.slice(1);
          }
          module.exports = JSONParse(code);
        } catch (err) {
          err.message = `${filename}: ${err.message}`;
          throw err;
        }
      };
    }

    this.started = true;
  }
}

module.exports = RelationalRequireCacheLoader;
