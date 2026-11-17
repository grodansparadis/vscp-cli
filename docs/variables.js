(function () {
  function documentationVariables(hook) {
    hook.doneEach(function () {
      const fields = document.querySelectorAll('[data-vscp-variable]');
      if (!fields.length) return;

      return fetch(new URL('variables.xml', document.baseURI), { cache: 'no-cache' })
        .then(function (response) {
          if (!response.ok) throw new Error('HTTP ' + response.status);
          return response.text();
        })
        .then(function (text) {
          const xml = new DOMParser().parseFromString(text, 'application/xml');
          const root = xml.documentElement;
          if (root.tagName !== 'var' || xml.getElementsByTagName('parsererror').length) {
            throw new Error('Invalid variables.xml');
          }

          const values = Array.from(fields, function (field) {
            const name = field.getAttribute('data-vscp-variable');
            const matches = Array.from(root.children).filter(function (child) {
              return child.tagName === name;
            });
            if (matches.length !== 1 || !matches[0].textContent.trim()) {
              throw new Error('Missing or invalid documentation variable: ' + name);
            }
            return matches[0].textContent.trim();
          });
          fields.forEach(function (field, index) {
            field.textContent = values[index];
          });
        })
        .catch(function (error) {
          console.error('Unable to load documentation variables:', error);
        });
    });
  }

  window.$docsify.plugins = [documentationVariables].concat(window.$docsify.plugins || []);
}());
