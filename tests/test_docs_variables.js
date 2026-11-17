'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const source = fs.readFileSync(path.join(__dirname, '../docs/variables.js'), 'utf8');

function field(name) {
  return {
    textContent: 'unavailable',
    getAttribute: function () { return name; }
  };
}

async function main() {
  let fields = [];
  let xml;
  let response = { ok: true, text: () => Promise.resolve('xml') };
  const requests = [];
  const errors = [];
  const existingPlugin = function () {};
  const window = { $docsify: { plugins: [existingPlugin] } };
  const document = {
    baseURI: 'https://example.test/vscp-cli/index.html',
    querySelectorAll: () => fields
  };
  class DOMParser {
    parseFromString(text, type) {
      assert.strictEqual(text, 'xml');
      assert.strictEqual(type, 'application/xml');
      return xml;
    }
  }
  vm.runInNewContext(source, {
    window, document, DOMParser, URL,
    fetch: (url, options) => {
      requests.push({ url: url.href, cache: options.cache });
      return Promise.resolve(response);
    },
    console: { error: (...args) => errors.push(args) }
  });

  let render;
  window.$docsify.plugins[0]({ doneEach: callback => { render = callback; } });
  assert.strictEqual(window.$docsify.plugins[1], existingPlugin);

  await render();
  assert.strictEqual(requests.length, 0);

  fields = [field('creation-time'), field('document-version')];
  xml = {
    documentElement: { tagName: 'var', children: [
      { tagName: 'creation-time', textContent: ' 2026-11-16 13:55 ' },
      { tagName: 'document-version', textContent: '26.11.8' }
    ] },
    getElementsByTagName: () => []
  };
  await render();
  assert.deepStrictEqual(fields.map(item => item.textContent), ['2026-11-16 13:55', '26.11.8']);
  assert.strictEqual(requests[0].url, 'https://example.test/vscp-cli/variables.xml');
  assert.strictEqual(requests[0].cache, 'no-cache');

  fields = [field('creation-time'), field('document-version')];
  xml.documentElement.children[1].textContent = '26.11.9';
  await render();
  assert.deepStrictEqual(fields.map(item => item.textContent), ['2026-11-16 13:55', '26.11.9']);

  fields = [field('creation-time'), field('document-version')];
  xml.getElementsByTagName = () => [{}];
  await render();
  assert.deepStrictEqual(fields.map(item => item.textContent), ['unavailable', 'unavailable']);
  assert(/Invalid variables.xml/.test(errors.pop()[1].message));

  xml.getElementsByTagName = () => [];
  xml.documentElement.children.pop();
  await render();
  assert.deepStrictEqual(fields.map(item => item.textContent), ['unavailable', 'unavailable']);
  assert(/Missing or invalid documentation variable: document-version/.test(errors.pop()[1].message));

  response = { ok: false, status: 404 };
  await render();
  assert.deepStrictEqual(fields.map(item => item.textContent), ['unavailable', 'unavailable']);
  assert(/HTTP 404/.test(errors.pop()[1].message));
  assert.strictEqual(errors.length, 0);
  console.log('Documentation variable tests passed');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
