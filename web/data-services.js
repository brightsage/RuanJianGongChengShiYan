'use strict';

const $ = (selector) => document.querySelector(selector);
const state = {token: '', principal: null, services: [], applications: [], users: [], roles: [], panel: '', offset: 0};
const prefix = '/api/modules/data-services';
const has = (permission) => Boolean(state.principal?.permissions.includes(permission));
const field = (form, name) => form.elements.namedItem(name);
const value = (form, name) => field(form, name).value.trim();
const integer = (form, name) => Number(value(form, name));
const pretty = (data) => JSON.stringify(data, null, 2);

function notice(message, error = false) {
  $('#notice').textContent = message;
  $('#notice').className = error ? 'error' : '';
}
function clearSession() {
  state.token = ''; state.principal = null; state.panel = '';
  for (const key of ['services', 'applications', 'users', 'roles']) state[key] = [];
  $('#workspace').hidden = true; $('#login-panel').hidden = false; $('#logout').hidden = true;
  $('#identity').textContent = ''; $('#token-value').textContent = ''; $('#issued-token').hidden = true;
  $('#invoke-form').reset(); $('#invoke-result').textContent = '尚未调用。'; $('#principal').textContent = '';
  for (const id of ['service-list', 'application-list', 'user-list', 'role-list', 'mask-list', 'log-list']) $(`#${id}`).replaceChildren();
}
async function api(path, method = 'GET', body, token = state.token) {
  const headers = {};
  if (token) headers.Authorization = `Bearer ${token}`;
  if (body !== undefined) headers['Content-Type'] = 'application/json';
  const response = await fetch(prefix + path, {method, headers, body: body === undefined ? undefined : JSON.stringify(body), cache: 'no-store'});
  const result = await response.json();
  if (!response.ok) {
    if (response.status === 401 && token === state.token && path !== '/login') clearSession();
    throw new Error(`${response.status} · ${result.message || result.error}`);
  }
  return result;
}
async function run(action) {
  try { await action(); } catch (error) { notice(error.message, true); }
}
function onSubmit(id, handler) {
  const form = $(`#${id}`);
  form.addEventListener('submit', (event) => {
    event.preventDefault();
    run(async () => {
      const submit = form.querySelector('button:not([type="button"])'); submit.disabled = true;
      try { await handler(form); } finally { submit.disabled = false; }
    });
  });
}
function action(label, callback) {
  const button = document.createElement('button'); button.type = 'button'; button.textContent = label;
  button.addEventListener('click', () => run(callback)); return button;
}
function table(id, columns, rows, actions) {
  const container = $(`#${id}`); container.replaceChildren();
  if (!rows.length) { const p = document.createElement('p'); p.textContent = '暂无记录。'; container.append(p); return; }
  const wrapper = document.createElement('div'); wrapper.className = 'scroll';
  const element = document.createElement('table'); const head = document.createElement('thead'); const tr = document.createElement('tr');
  for (const column of columns) { const th = document.createElement('th'); th.textContent = column[1]; tr.append(th); }
  if (actions) { const th = document.createElement('th'); th.textContent = '操作'; tr.append(th); }
  head.append(tr); element.append(head); const body = document.createElement('tbody');
  for (const row of rows) {
    const line = document.createElement('tr');
    for (const [key] of columns) {
      const cell = document.createElement('td'); const data = row[key];
      cell.textContent = data == null ? '—' : typeof data === 'object' ? JSON.stringify(data) : String(data); line.append(cell);
    }
    if (actions) { const cell = document.createElement('td'); for (const button of actions(row)) cell.append(button); line.append(cell); }
    body.append(line);
  }
  element.append(body); wrapper.append(element); container.append(wrapper);
}
function scopeWidget(container) {
  const options = [['scope_type', '范围', ['all', 'college', 'class', 'self']], ['org_id', '组织 ID'], ['class_id', '班级 ID'], ['student_no', '学号']];
  for (const [name, title, choices] of options) {
    const label = document.createElement('label'); label.textContent = title;
    const input = document.createElement(choices ? 'select' : 'input'); input.name = name;
    if (choices) for (const choice of choices) { const option = document.createElement('option'); option.value = choice; option.textContent = choice; input.append(option); }
    label.append(input); container.append(label);
  }
  const update = () => {
    const type = container.querySelector('[name="scope_type"]').value;
    for (const name of ['org_id', 'class_id', 'student_no']) {
      const input = container.querySelector(`[name="${name}"]`);
      const required = name === 'org_id' ? type !== 'all' : name === 'class_id' ? ['class', 'self'].includes(type) : type === 'self';
      input.required = required; input.closest('label').hidden = !required;
    }
  };
  container.querySelector('select').addEventListener('change', update); update();
}
for (const container of document.querySelectorAll('.scope')) scopeWidget(container);
function setScope(form, scope) {
  field(form, 'scope_type').value = scope.type;
  for (const name of ['org_id', 'class_id', 'student_no']) field(form, name).value = scope[name] || '';
  field(form, 'scope_type').dispatchEvent(new Event('change'));
}
function getScope(form) {
  const type = value(form, 'scope_type'), scope = {type};
  if (type !== 'all') scope.org_id = value(form, 'org_id');
  if (['class', 'self'].includes(type)) scope.class_id = value(form, 'class_id');
  if (type === 'self') scope.student_no = value(form, 'student_no');
  return scope;
}
function renderPermissions(permissions) {
  const checked = [...$('#permission-options').querySelectorAll('input:checked')].map((input) => input.value);
  $('#permission-options').replaceChildren();
  for (const permission of permissions) {
    const label = document.createElement('label'); label.className = 'inline'; label.title = permission.name;
    const input = document.createElement('input'); input.type = 'checkbox'; input.name = 'permissions'; input.value = permission.code;
    input.checked = checked.includes(permission.code);
    label.append(input, document.createTextNode(permission.code)); $('#permission-options').append(label);
  }
}
const panels = [['services', '数据服务', 'services.read'], ['applications', '授权申请', ['grants.apply', 'grants.approve']], ['users', '用户', 'users.manage'], ['roles', '角色', 'roles.manage'], ['masking', '脱敏', 'masking.manage'], ['logs', '审计', 'audit.read'], ['account', '当前身份', null]];
function selectPanel(id) {
  state.panel = id;
  for (const section of document.querySelectorAll('[data-panel]')) section.hidden = section.dataset.panel !== id;
  for (const button of $('#tabs').children) button.setAttribute('aria-current', button.dataset.id === id ? 'page' : 'false');
  if (id === 'logs') run(loadLogs);
}
function renderNavigation() {
  $('#tabs').replaceChildren();
  const available = panels.filter(([, , permission]) => !permission || (Array.isArray(permission) ? permission.some(has) : has(permission)));
  for (const [id, label] of available) { const button = action(label, () => selectPanel(id)); button.dataset.id = id; $('#tabs').append(button); }
  for (const element of document.querySelectorAll('[data-permission]')) element.hidden = !has(element.dataset.permission);
  selectPanel(available.some(([id]) => id === state.panel) ? state.panel : available[0][0]);
}
function resetUser() {
  const form = $('#user-form'); form.reset(); field(form, 'user_id').value = '';
  field(form, 'username').disabled = false; field(form, 'password').required = true;
  field(form, 'active').disabled = true;
  $('#user-form-title').textContent = '创建用户';
}
async function refresh() {
  const principal = await api('/me'); state.principal = principal;
  $('#identity').textContent = `${principal.username} · ${principal.role}`;
  $('#principal').textContent = pretty(principal); $('#workspace').hidden = false; $('#login-panel').hidden = true; $('#logout').hidden = false;
  const requests = [];
  if (has('services.read')) requests.push(api('/services').then((rows) => { state.services = rows; }));
  if (has('grants.apply') || has('grants.approve')) requests.push(api('/applications').then((rows) => { state.applications = rows; }));
  if (has('users.manage')) requests.push(api('/users').then((rows) => { state.users = rows; }));
  if (has('roles.manage') || has('users.manage')) requests.push(api('/roles').then((rows) => { state.roles = rows; }));
  if (has('roles.manage') || has('users.manage')) requests.push(api('/permissions').then(renderPermissions));
  if (has('masking.manage')) requests.push(api('/masking-rules').then((rows) => {
    table('mask-list', [['field', '字段'], ['algorithm', '算法']], rows, (row) => [action('编辑', () => {
      field($('#mask-form'), 'field').value = row.field; field($('#mask-form'), 'algorithm').value = row.algorithm;
    })]);
  }));
  if (has('services.publish')) requests.push(api('/providers').then((providers) => {
    $('#providers').textContent = providers.length ? `已接入提供者：${providers.map((p) => p.id).join('、')}` : '尚未接入数据提供者。可先发布配置；调用将在提供者接入后可用。';
    $('#provider-options').replaceChildren();
    for (const provider of providers) { const option = document.createElement('option'); option.value = provider.id; option.label = provider.fields.join(', '); $('#provider-options').append(option); }
  }));
  await Promise.all(requests);
  table('service-list', [['name', '名称'], ['version', '版本'], ['provider_id', '提供者'], ['fields', '字段'], ['scope', '范围'], ['rate_per_minute', '每分钟上限'], ['active', '启用']], state.services,
    has('services.publish') ? (row) => [action(row.active ? '暂停' : '恢复', async () => { await api(`/services/${row.id}`, 'PATCH', {active: !row.active}); await refresh(); notice('服务状态已更新。'); })] : undefined);
  const select = field($('#application-form'), 'service_id'), previous = select.value; select.replaceChildren();
  for (const service of state.services.filter((row) => row.active)) { const option = document.createElement('option'); option.value = service.id; option.textContent = `${service.name} / ${service.version}`; select.append(option); }
  if ([...select.options].some((option) => option.value === previous)) select.value = previous;
  table('application-list', [['id', '申请 ID'], ['service_id', '服务 ID'], ['purpose', '用途'], ['scope', '范围'], ['status', '状态'], ['reason', '审批意见'], ['grant', '授权详情']], state.applications, (row) => {
    const buttons = [];
    if (has('grants.approve') && row.status === 'pending') buttons.push(action('审批', () => {
      const form = $('#decision-form'); form.reset(); field(form, 'id').value = row.id;
      for (const name of ['validity_seconds', 'quota', 'rate_per_minute']) { field(form, name).value = row[name]; field(form, name).max = row[name]; }
      $('#decision-dialog').showModal();
    }));
    if (row.grant && row.status === 'approved') {
      if (row.owner_id === principal.user_id) buttons.push(action('签发 / 轮换令牌', async () => {
        const result = await api(`/applications/${row.id}/token`, 'POST'); $('#token-value').textContent = result.access_token; $('#issued-token').hidden = false;
        field($('#invoke-form'), 'token').value = result.access_token; field($('#invoke-form'), 'service_id').value = row.service_id;
        notice('新令牌已签发，旧令牌即时失效。');
      }));
      buttons.push(action('撤销授权', async () => { await api(`/applications/${row.id}/revoke`, 'POST'); $('#issued-token').hidden = true; $('#token-value').textContent = ''; await refresh(); notice('授权已撤销。'); }));
    }
    return buttons;
  });
  table('user-list', [['username', '用户名'], ['role', '角色'], ['org_id', '组织'], ['class_id', '班级'], ['student_no', '学号'], ['active', '启用']], state.users, (row) => [action('编辑', () => {
    const form = $('#user-form'); resetUser();
    for (const name of ['user_id', 'username', 'role', 'org_id', 'class_id']) field(form, name).value = row[name];
    field(form, 'username').disabled = true; field(form, 'password').required = false;
    field(form, 'active').disabled = false; field(form, 'active').checked = row.active;
    $('#user-form-title').textContent = `编辑 ${row.username}`;
  })]);
  const roleSelect = field($('#user-form'), 'role'), previousRole = roleSelect.value; roleSelect.replaceChildren();
  for (const role of state.roles) { const option = document.createElement('option'); option.value = role.code; option.textContent = role.name; roleSelect.append(option); }
  if ([...roleSelect.options].some((option) => option.value === previousRole)) roleSelect.value = previousRole;
  table('role-list', [['code', '编码'], ['name', '名称'], ['scope_type', '范围'], ['permissions', '权限']], state.roles, (row) => row.code === 'system_admin' ? [] : [action('编辑', () => {
    const form = $('#role-form'); for (const name of ['code', 'name', 'scope_type']) field(form, name).value = row[name];
    for (const input of form.querySelectorAll('[name="permissions"]')) input.checked = row.permissions.includes(input.value);
  })]);
  renderNavigation();
}

onSubmit('login-form', async (form) => {
  const result = await api('/login', 'POST', {username: value(form, 'username'), password: field(form, 'password').value}, '');
  state.token = result.access_token; form.reset(); await refresh(); setScope($('#application-form'), state.principal.scope);
  notice('登录成功。');
});
$('#logout').addEventListener('click', () => run(async () => { await api('/logout', 'POST'); clearSession(); notice('已退出登录。'); }));
onSubmit('service-form', async (form) => {
  await api('/services', 'POST', {name: value(form, 'name'), version: value(form, 'version'), provider_id: value(form, 'provider_id'),
    fields: value(form, 'fields').split(',').map((s) => s.trim()).filter(Boolean), scope: getScope(form), rate_per_minute: integer(form, 'rate_per_minute')});
  await refresh(); notice('新版本已发布。');
});
onSubmit('application-form', async (form) => {
  await api('/applications', 'POST', {service_id: value(form, 'service_id'), purpose: value(form, 'purpose'), scope: getScope(form),
    validity_seconds: integer(form, 'validity_seconds'), quota: integer(form, 'quota'), rate_per_minute: integer(form, 'rate_per_minute')});
  await refresh(); notice('申请已提交。');
});
onSubmit('decision-form', async (form) => {
  const body = {decision: value(form, 'decision')}; const reason = value(form, 'reason');
  if (reason) body.reason = reason;
  if (body.decision === 'approve') Object.assign(body, {validity_seconds: integer(form, 'validity_seconds'), quota: integer(form, 'quota'), rate_per_minute: integer(form, 'rate_per_minute'), mask_level: value(form, 'mask_level')});
  else if (!reason) throw new Error('拒绝申请时请填写原因。');
  await api(`/applications/${value(form, 'id')}/decision`, 'POST', body); $('#decision-dialog').close(); await refresh(); notice('审批已完成。');
});
$('#cancel-decision').addEventListener('click', () => $('#decision-dialog').close());
onSubmit('invoke-form', async (form) => {
  const parameters = JSON.parse(value(form, 'parameters'));
  try { const result = await api(`/services/${encodeURIComponent(value(form, 'service_id'))}/invoke`, 'POST', {parameters}, field(form, 'token').value); $('#invoke-result').textContent = pretty(result); notice('调用成功。'); await refresh(); }
  catch (error) { $('#invoke-result').textContent = error.message; throw error; }
});
onSubmit('user-form', async (form) => {
  const id = value(form, 'user_id'), body = {role: value(form, 'role')};
  if (!id) body.username = value(form, 'username');
  if (field(form, 'password').value) body.password = field(form, 'password').value;
  for (const name of ['org_id', 'class_id', 'student_no']) if (value(form, name)) body[name] = value(form, name);
  if (id) body.active = field(form, 'active').checked;
  await api(id ? `/users/${id}` : '/users', id ? 'PATCH' : 'POST', body); resetUser(); await refresh(); notice('用户已保存；修改用户后其会话会失效。');
});
$('#reset-user').addEventListener('click', resetUser);
onSubmit('role-form', async (form) => {
  await api(`/roles/${encodeURIComponent(value(form, 'code'))}`, 'PUT', {name: value(form, 'name'), scope_type: value(form, 'scope_type'), permissions: [...form.querySelectorAll('[name="permissions"]:checked')].map((input) => input.value)});
  await refresh(); notice('角色已保存。');
});
onSubmit('mask-form', async (form) => {
  await api(`/masking-rules/${encodeURIComponent(value(form, 'field'))}`, 'PUT', {algorithm: value(form, 'algorithm')}); await refresh(); notice('脱敏规则已保存。');
});
async function loadLogs() {
  const form = $('#log-form'), kind = value(form, 'kind'), query = new URLSearchParams({limit: '20', offset: String(state.offset)});
  for (const name of ['actor_id', ...(kind === 'audit-logs' ? ['action'] : ['service_id', 'status'])]) if (value(form, name)) query.set(name, value(form, name));
  for (const name of ['from', 'to']) if (value(form, name)) query.set(name, String(Math.floor(new Date(value(form, name)).getTime() / 1000)));
  const rows = await api(`/${kind}?${query}`);
  const columns = kind === 'audit-logs' ? [['created_at', 'Unix 时间'], ['actor_id', '操作人'], ['action', '动作'], ['resource_id', '资源'], ['outcome', '结果'], ['reason', '原因'], ['before', '变更前'], ['after', '变更后']] : [['created_at', 'Unix 时间'], ['actor_id', '调用方'], ['service_id', '服务'], ['status', '状态码'], ['reason', '原因'], ['request_id', '请求 ID']];
  table('log-list', columns, rows); $('#page').textContent = `第 ${state.offset / 20 + 1} 页`;
  $('#previous').disabled = state.offset === 0; $('#next').disabled = rows.length < 20;
}
onSubmit('log-form', async () => { state.offset = 0; await loadLogs(); });
$('#previous').addEventListener('click', () => run(async () => { state.offset = Math.max(0, state.offset - 20); await loadLogs(); }));
$('#next').addEventListener('click', () => run(async () => { state.offset += 20; await loadLogs(); }));
notice('请登录以访问账号权限内的管理功能。');
