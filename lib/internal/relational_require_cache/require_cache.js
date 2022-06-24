'use strict';

/* eslint-disable max-len */

const { Error } = primordials;

const BaseObject = require('internal/relational_require_cache/base_object');
const Module = require('module');
const path = require('path');
const vm = require('vm');

let singleton;

class RequireCache extends BaseObject {
  constructor(entries, filename) {
    if (singleton) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('only one RequireCacheReader or RequireCacheWriter can be created in one process.');
    }

    super(entries, filename);
    singleton = this;
  }

  transformFilename(filename) {
    if (typeof filename !== 'string') {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('filename should be a string.');
    }

    return this.cache.transformFilename(filename);
  }

  transformFilenameBack(filename) {
    if (typeof filename !== 'string') {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('filename should be a string.');
    }

    return this.cache.transformFilenameBack(filename);
  }
}

class ExternalRequireCacheReader extends RequireCache {
  constructor(filename, entries) {
    super(entries, filename);
  }

  queryRelation(parent, request) {
    if (!path.isAbsolute(parent)) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('parent filename should be absolute');
    }

    const transformedParent = this.transformFilename(parent);
    const transformedRequest = path.isAbsolute(request) ? this.transformFilename(request) : request;
    const cachedPath = this.cache.queryRelation(transformedParent, transformedRequest);
    return cachedPath ? this.transformFilenameBack(cachedPath) : null;
  }

  getCode(filename) {
    if (!path.isAbsolute(filename)) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('filename should be absolute.');
    }

    const dummyFilename = this.transformFilename(filename);
    return this.cache.queryCode(dummyFilename);
  }
}

class ExternalRequireCacheWriter extends RequireCache {
  constructor(entries) {
    super(entries, undefined);
  }

  addRelation(parent, request, target) {
    if (!path.isAbsolute(parent) || !path.isAbsolute(target)) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('parent and target filename should be absolute');
    }

    const transformedParent = this.cache.transformFilename(parent);
    const transformedTarget = this.cache.transformFilename(target);
    const transformedRequest = path.isAbsolute(request) ? this.transformFilename(request) : request;

    this.cache.addRelation(transformedParent, transformedRequest, transformedTarget);
  }

  addCode(filename, code, options = {}) {
    if (!path.isAbsolute(filename)) {
      // eslint-disable-next-line no-restricted-syntax
      throw new Error('filename should be absolute.');
    }

    let buff = null;
    const dummyFilename = this.transformFilename(filename);

    if (options.shouldWrap) {
      code = Module.wrap(code);
    }

    if (options.bytecode) {
      const script = new vm.Script(code, {
        filename,
        lineOffset: 0,
        displayErrors: true,
      });

      buff = script.createCachedData();
    }

    this.cache.addCode(dummyFilename, code, buff);
  }

  dump(filename) {
    return this.cache.dump(filename);
  }
}

module.exports = {
  Reader: ExternalRequireCacheReader,
  Writer: ExternalRequireCacheWriter,
};
