// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
// Test compressing and uncompressing a string with zlib

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing eli' +
                    't. Morbi faucibus, purus at gravida dictum, libero arcu ' +
                    'convallis lacus, in commodo libero metus eu nisi. Nullam' +
                    ' commodo, neque nec porta placerat, nisi est fermentum a' +
                    'ugue, vitae gravida tellus sapien sit amet tellus. Aenea' +
                    'n non diam orci. Proin quis elit turpis. Suspendisse non' +
                    ' diam ipsum. Suspendisse nec ullamcorper odio. Vestibulu' +
                    'm arcu mi, sodales non suscipit id, ultrices ut massa. S' +
                    'ed ac sem sit amet arcu malesuada fermentum. Nunc sed. ';

// defate result changed with zlib-cloudflare
const expectedBase64Deflate = 'eJxdkN1pA0EMhFuZAo7rIe9JCBjyLu/KRrC7WuvHPbkZ1xTuEtuQV2lmmPnut/vtXY07ZHp2VG1qcAlQ51hQdDiX4EgDVZniRcYZ3CRWfKgdBSfKIsf0BTMtHRQ4G12lEqqUyL6gyZFNQVZyS7xSa+JoVDaXDBTtXas+dJ0jHZwY4rLiM1uj/hAtGHxJxuCCqRaE2aiwUSy7HuyBE1vnEdlBeU5ecJUgfvYKbi0dTlN4PNf+nVe88WAaGDpQhTrUiqz4MpWBS4rv8xFpU3zFIX3yqOLOL8tO89+PC/YlRW2yQavoim/2kGO2reqGp8sC10qNfU/z9CJTAlIXZAuTwo4MdHKnFQeuoALn/trxG7RFJFV6wdhQjk1bV/wAzvy5fQ==';
const expectedBase64Gzip = 'H4sIAAAAAAAAA11RS05DMQy8yhzg6d2BPSAkJPZu4laWkjiN4' +
                           '96pl+mZcAotEpss7PH8crverq86uEK6eUXWogMmE1R5bkjajN' +
                           'Pk6QOUpYslaSdwkbnjTcdBcCRPcnDb0H24gSZOgy6SCVnS9Lq' +
                           'hyIGHgkbyxXihUsRQKK0raTGrVbM+cKEcPOxoYrLj3Uuh+gBt' +
                           'aHx2jjeh65iEHkQ8KNwuPNgmjjwqt9AG+cl5w0Um8dPX5FJCw' +
                           'agLt2fa3/GOF25MDU1bJAhlHSlsfAwNq2cP5ys+opKoY8enW+' +
                           'eWxYz/Tu5t/tuF4XuSpKPzgGbRHV9hN9ory+qqp8oG00yF7c5' +
                           'mHo33kJO8xfkckmLjE5XMKBQ4gxIsfvCZ44doUThF2mcZq8q2' +
                           'sHnHNzRtagj5AQAA';

zlib.deflate(inputString, common.mustCall((err, buffer) => {
  assert.strictEqual(buffer.toString('base64'), expectedBase64Deflate);
}));

zlib.gzip(inputString, common.mustCall((err, buffer) => {
  // Can't actually guarantee that we'll get exactly the same
  // deflated bytes when we compress a string, since the header
  // depends on stuff other than the input string itself.
  // However, decrypting it should definitely yield the same
  // result that we're expecting, and this should match what we get
  // from inflating the known valid deflate data.
  zlib.gunzip(buffer, common.mustCall((err, gunzipped) => {
    assert.strictEqual(gunzipped.toString(), inputString);
  }));
}));

let buffer = Buffer.from(expectedBase64Deflate, 'base64');
zlib.unzip(buffer, common.mustCall((err, buffer) => {
  assert.strictEqual(buffer.toString(), inputString);
}));

buffer = Buffer.from(expectedBase64Gzip, 'base64');
zlib.unzip(buffer, common.mustCall((err, buffer) => {
  assert.strictEqual(buffer.toString(), inputString);
}));
