const modulesElement = document.querySelector('#modules');
const catalogStatus = document.querySelector('#catalog-status');
const form = document.querySelector('#request-form');
const methodInput = document.querySelector('#method');
const pathInput = document.querySelector('#path');
const headerInput = document.querySelector('#headers');
const bodyInput = document.querySelector('#body');
const responseStatus = document.querySelector('#response-status');
const responseBody = document.querySelector('#response-body');
const demoButton = document.querySelector('#load-demo');
const demoStatus = document.querySelector('#demo-status');
const demoProfile = document.querySelector('#demo-profile');

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

function renderDemoProfile(profile) {
  demoProfile.replaceChildren();

  const title = document.createElement('h3');
  title.textContent = `${profile.student.name}（${profile.student.student_id}）`;
  const meta = document.createElement('p');
  meta.className = 'muted';
  meta.textContent = `${profile.student.major} · ${profile.student.class_name} · ${profile.term}`;
  demoProfile.append(title, meta);

  const metricsTitle = document.createElement('h4');
  metricsTitle.textContent = `指标（${profile.metrics.length}）`;
  demoProfile.append(metricsTitle);
  if (profile.metrics.length === 0) {
    const empty = document.createElement('p');
    empty.className = 'muted';
    empty.textContent = '暂无指标数据。';
    demoProfile.append(empty);
  } else {
    const table = document.createElement('table');
    const head = document.createElement('tr');
    for (const label of ['指标', '数值', '单位', '来源']) {
      const cell = document.createElement('th');
      cell.textContent = label;
      head.append(cell);
    }
    const thead = document.createElement('thead');
    thead.append(head);
    table.append(thead);
    const tbody = document.createElement('tbody');
    for (const metric of profile.metrics) {
      const row = document.createElement('tr');
      for (const value of [metric.metric_code, metric.value, metric.unit, metric.source]) {
        const cell = document.createElement('td');
        cell.textContent = value;
        row.append(cell);
      }
      tbody.append(row);
    }
    table.append(tbody);
    demoProfile.append(table);
  }

  const tagsTitle = document.createElement('h4');
  tagsTitle.textContent = `标签（${profile.tags.length}）`;
  demoProfile.append(tagsTitle);
  if (profile.tags.length === 0) {
    const empty = document.createElement('p');
    empty.className = 'muted';
    empty.textContent = '暂无标签。';
    demoProfile.append(empty);
  } else {
    const list = document.createElement('ul');
    for (const tag of profile.tags) {
      const item = document.createElement('li');
      item.textContent = `${tag.name}（${tag.dimension}）`;
      const evidence = document.createElement('div');
      evidence.className = 'evidence';
      evidence.textContent = JSON.stringify(tag.evidence);
      item.append(evidence);
      list.append(item);
    }
    demoProfile.append(list);
  }

  const report = document.createElement('a');
  report.href = `/api/modules/student-profile/students/${encodeURIComponent(
    profile.student.student_id,
  )}/report?term=${encodeURIComponent(profile.term)}`;
  report.target = '_blank';
  report.textContent = '打开 HTML 画像报告';
  demoProfile.append(report);
}

async function loadDemoProfile() {
  demoStatus.textContent = '加载中…';
  demoProfile.replaceChildren();
  try {
    const response = await fetch(
      '/api/modules/student-profile/students/2023001?term=2026-fall',
    );
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const profile = await response.json();
    demoStatus.textContent = '数据来自真实接口，下面的内容就是接口返回的画像。';
    renderDemoProfile(profile);
  } catch (error) {
    demoStatus.textContent = `加载失败：${error.message}`;
  }
}

loadModules();
if (demoButton && demoStatus && demoProfile) {
  demoButton.addEventListener('click', loadDemoProfile);
  loadDemoProfile();
}
