'use strict';

/* eslint-disable max-len */

require('../common');

const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const { RequireCacheReader, RequireCacheWriter } = require('strontium/relational_require_cache');
const vm = require('vm');

if (process.argv[2] === 'writer') {
  const writer = new RequireCacheWriter([ '/tmp', '/etc' ]);
  writer.addRelation('/tmp/a.js', 'b.js', '/etc/b.js');
  writer.addRelation('/etc/a.js', 'b.js', '/tmp/b.js');
  writer.addRelation('/a.js', 'b.js', '/b.js');
  writer.addCode('/etc/b.js', 'var a = 1;');
  writer.addCode('/tmp/b.js', 'var a = 1;', { shouldWrap: true, bytecode: true });
  writer.addCode('/b.js', 'var b = 1;', { shouldWrap: true, bytecode: false });
  writer.dump(path.join(__dirname, 'temp.strrrc'));

  assert.throws(() => {
    new RequireCacheReader([ '/tmp/etc' ]);
  }, /only one RequireCacheReader or RequireCacheWriter can be created in one process\./);

  return;
} else if (process.argv[2] === 'reader') {
  const reader = new RequireCacheReader(path.join(__dirname, 'temp.strrrc'), [ '/hello', '/world' ]);

  assert.strictEqual(reader.queryRelation('/hello/a.js', 'b.js'), '/world/b.js');
  assert.strictEqual(reader.queryRelation('/world/a.js', 'b.js'), '/hello/b.js');
  assert.strictEqual(reader.queryRelation('/a.js', 'b.js'), '/b.js');
  assert.strictEqual(reader.queryRelation('/hello/a.js', 'a.js'), null);

  assert.deepStrictEqual(reader.getCode('/world/b.js'), {
    sourcecode: 'var a = 1;',
    bytecode: null,
  });

  assert.deepStrictEqual(reader.getCode('/b.js'), {
    sourcecode: '(function (exports, require, module, __filename, __dirname) { var b = 1;\n});',
    bytecode: null,
  });

  const temp = reader.getCode('/hello/b.js');
  assert.strictEqual(temp.sourcecode, '(function (exports, require, module, __filename, __dirname) { var a = 1;\n});');

  const script = new vm.Script(temp.sourcecode, {
    filename: '/hello/b.js',
    lineOffset: 0,
    displayErrors: true,
    cachedData: temp.bytecode,
  });
  assert(!script.cachedDataRejected);

  assert.strictEqual(reader.getCode('/a', 'b'), null);

  process.on('exit', () => {
    fs.unlinkSync(path.join(__dirname, 'temp.strrrc'));
  });

  return;
}

let ret = cp.spawnSync(process.execPath, [ __filename, 'writer' ]);
assert.strictEqual(ret.status, 0, ret.stderr.toString());

ret = cp.spawnSync(process.execPath, [ __filename, 'reader' ]);
assert.strictEqual(ret.status, 0, ret.stderr.toString());
