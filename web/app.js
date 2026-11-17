const modulesElement = document.querySelector('#modules');
const catalogStatus = document.querySelector('#catalog-status');
const form = document.querySelector('#request-form');
const methodInput = document.querySelector('#method');
const pathInput = document.querySelector('#path');
const headerInput = document.querySelector('#headers');
const bodyInput = document.querySelector('#body');
const responseStatus = document.querySelector('#response-status');
const responseBody = document.querySelector('#response-body');

function chooseRoute(method, path) {
  methodInput.value = method;
  pathInput.value = path;
  pathInput.focus();
}

async function loadModules() {
  try {
    const response = await fetch('/api/modules');
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const modules = await response.json();
    catalogStatus.textContent = `已加载 ${modules.length} 个模块。路由由各模块开发者添加。`;
    modulesElement.replaceChildren();
    for (const module of modules) {
      const card = document.createElement('article');
      const title = document.createElement('h3');
      title.textContent = module.name;
      const id = document.createElement('code');
      id.textContent = module.id;
      card.append(title, id);
      if (module.routes.length === 0) {
        const empty = document.createElement('p');
        empty.textContent = '尚无业务路由';
        card.append(empty);
      }
      for (const route of module.routes) {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'route';
        button.textContent = `${route.method} ${route.path}`;
        button.addEventListener('click', () => chooseRoute(route.method, route.path));
        card.append(button);
      }
      modulesElement.append(card);
    }
  } catch (error) {
    catalogStatus.textContent = `读取模块失败：${error.message}`;
  }
}

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  responseStatus.textContent = '请求中…';
  responseBody.textContent = '';
  try {
    const headers = {};
    for (const line of headerInput.value.split('\n')) {
      if (!line.trim()) continue;
      const colon = line.indexOf(':');
      if (colon < 1) throw new Error(`请求头格式错误：${line}`);
      headers[line.slice(0, colon).trim()] = line.slice(colon + 1).trim();
    }
    const path = pathInput.value.trim();
    if (!path.startsWith('/') || path.startsWith('//')) throw new Error('路径必须以单个 / 开头');
    const method = methodInput.value;
    const options = {method, headers};
    if (method !== 'GET' && bodyInput.value) options.body = bodyInput.value;
    const response = await fetch(path, options);
    const body = await response.text();
    responseStatus.textContent = `${response.status} ${response.statusText}`;
    try {
      responseBody.textContent = JSON.stringify(JSON.parse(body), null, 2);
    } catch {
      responseBody.textContent = body || '（空响应）';
    }
  } catch (error) {
    responseStatus.textContent = '请求失败';
    responseBody.textContent = error.message;
  }
});

loadModules();
