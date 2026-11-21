const API = '/api/modules';

const state = {
  tab: 'overview',
  loaded: new Set(),
  sources: [],
  tasks: [],
  students: [],
  selectedStudent: null,
  runTaskId: null,
  alertId: null,
  alerts: [],
  token: '',
  username: '',
  pendingTab: null,
  ruleRows: [],
  masterRows: [],
  alertRules: [],
  alertDashboard: {},
  alertTotal: 0,
  alertOverdue: 0,
  pages: { master: 1, alerts: 1 },
  alertFilter: { status: 'open', level: '', className: '' },
};

const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));
let toastTimer = 0;

function escapeHtml(value) {
  const map = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
  return String(value ?? '').replace(/[&<>"']/g, (character) => map[character]);
}

function badge(label, tone = 'neutral') {
  return `<span class="badge ${tone}">${escapeHtml(label)}</span>`;
}

function statusTone(status) {
  return ({
    success: 'success',
    partial: 'warn',
    failed: 'danger',
    running: 'info',
    pending: 'neutral',
    open: 'danger',
    claimed: 'warn',
    closed: 'success',
    not_applicable: 'neutral',
  })[status] || 'neutral';
}

function levelTone(level) {
  return ({ red: 'danger', orange: 'warn', yellow: 'yellow' })[level] || 'neutral';
}

function empty(message) {
  return `<p class="empty">${escapeHtml(message)}</p>`;
}

const PAGE_SIZE = 8;

function pageSlice(list, key) {
  const total = list.length;
  const totalPages = Math.max(1, Math.ceil(total / PAGE_SIZE));
  const page = Math.min(Math.max(1, state.pages[key] || 1), totalPages);
  state.pages[key] = page;
  const start = (page - 1) * PAGE_SIZE;
  return { items: list.slice(start, start + PAGE_SIZE), page, totalPages, total };
}

function renderPager(selector, info, key) {
  const container = $(selector);
  if (!container) return;
  if (info.totalPages <= 1) { container.innerHTML = ''; return; }
  container.dataset.pagerKey = key;
  container.innerHTML = `<span class="hint">第 ${info.page}/${info.totalPages} 页 · 共 ${info.total} 条</span>
    <button class="btn ghost small" data-page="${info.page - 1}" ${info.page <= 1 ? 'disabled' : ''}>上一页</button>
    <button class="btn ghost small" data-page="${info.page + 1}" ${info.page >= info.totalPages ? 'disabled' : ''}>下一页</button>`;
}

function ruleTypeLabel(type) {
  return ({ not_null: '非空', range: '值域', format: '格式' })[type] || type;
}

function qualityLevelLabel(level) {
  return ({ error: '错误', warn: '警告', info: '提示' })[level] || level;
}

function qualityLevelTone(level) {
  return ({ error: 'danger', warn: 'warn', info: 'info' })[level] || 'neutral';
}

function showToast(message, isError = false) {
  const toast = $('#toast');
  toast.textContent = message;
  toast.classList.toggle('error', isError);
  toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toast.hidden = true; }, 3600);
}

let busyCount = 0;

function setBusy(delta) {
  busyCount = Math.max(0, busyCount + delta);
  document.body.classList.toggle('busy', busyCount > 0);
}

async function api(path, options = {}) {
  const config = { ...options, headers: { ...(options.headers || {}) } };
  if (state.token) config.headers.Authorization = `Bearer ${state.token}`;
  if (config.body !== undefined && config.body !== null && typeof config.body !== 'string') {
    config.body = JSON.stringify(config.body);
    config.headers['Content-Type'] = 'application/json; charset=utf-8';
  }
  setBusy(1);
  try {
    const response = await fetch(path, config);
    const text = await response.text();
    let data = null;
    try { data = text ? JSON.parse(text) : null; } catch { data = text; }
    if (!response.ok) {
      // 平台错误体是 {error:"错误码", message:"中文原因"}，优先展示 message。
      const message = data && typeof data === 'object'
        ? (data.message || data.error || `HTTP ${response.status}`)
        : (String(data || '').trim() || `HTTP ${response.status}`);
      const error = new Error(message);
      error.status = response.status;
      throw error;
    }
    return data;
  } finally {
    setBusy(-1);
  }
}

function unwrapM2(value) {
  if (value && typeof value === 'object' && !Array.isArray(value) && 'data' in value) return value.data;
  return value;
}

function kindLabel(kind) {
  return ({ academic: '教务', research: '科研', practice: '实践' })[kind] || kind;
}

function modeLabel(mode) {
  return ({ file: '文件', push: '推送', api: '接口', manual: '手动', scheduled: '定时' })[mode] || mode;
}

function requiresLogin(tab) {
  return tab === 'ingestion' || tab === 'governance' || tab === 'profile' || tab === 'alerts';
}

function updateIdentity() {
  const identity = $('#identity');
  const loginButton = $('#login-button');
  const logoutButton = $('#logout-button');
  if (state.token) {
    identity.textContent = `已登录 ${state.username || ''}`.trim();
    identity.className = 'badge success';
    loginButton.hidden = true;
    logoutButton.hidden = false;
  } else {
    identity.textContent = '未登录';
    identity.className = 'badge neutral';
    loginButton.hidden = false;
    logoutButton.hidden = true;
  }
}

function openLogin(pendingTab = null) {
  if (state.token) {
    showToast('已经登录。');
    return;
  }
  state.pendingTab = pendingTab;
  const dialog = $('#login-dialog');
  if (!dialog.open) dialog.showModal();
  $('#login-password').focus();
}

async function submitLogin(event) {
  event.preventDefault();
  const username = $('#login-username').value.trim();
  const password = $('#login-password').value;
  try {
    const data = await api('/api/modules/data-services/login', {
      method: 'POST',
      body: { username, password },
    });
    state.token = data.access_token;
    state.username = username;
    $('#login-password').value = '';
    $('#login-dialog').close();
    updateIdentity();
    const pending = state.pendingTab;
    state.pendingTab = null;
    if (pending) {
      switchTab(pending);
    } else {
      state.loaded.delete(state.tab);
      tabInfo[state.tab].loader().catch((error) => showToast(error.message, true));
    }
    showToast('登录成功。');
  } catch (error) {
    showToast(error.message, true);
  }
}

function logout() {
  state.token = '';
  state.username = '';
  updateIdentity();
  state.loaded.delete('overview');
  switchTab('overview');
  showToast('已退出登录。');
}

const tabInfo = {
  overview: { title: '系统概览', crumb: '工作台 / 系统概览', loader: loadOverview },
  ingestion: { title: '数据接入', crumb: '工作台 / 数据接入', loader: loadIngestion },
  governance: { title: '数据治理', crumb: '工作台 / 数据治理', loader: loadGovernance },
  profile: { title: '学生画像', crumb: '工作台 / 学生画像', loader: loadProfile },
  alerts: { title: '学业预警', crumb: '工作台 / 学业预警', loader: loadAlerts },
};

function switchTab(tab) {
  if (!tabInfo[tab]) return;
  if (requiresLogin(tab) && !state.token) {
    openLogin(tab);
    return;
  }
  state.tab = tab;
  $$('.nav-item').forEach((button) => button.classList.toggle('active', button.dataset.tab === tab));
  $$('.panel').forEach((panel) => panel.classList.toggle('active', panel.id === `panel-${tab}`));
  $('#page-title').textContent = tabInfo[tab].title;
  $('#crumb').textContent = tabInfo[tab].crumb;
  if (!state.loaded.has(tab)) {
    state.loaded.add(tab);
    tabInfo[tab].loader().catch((error) => {
      state.loaded.delete(tab);
      if (error.status === 401) {
        state.token = '';
        state.username = '';
        updateIdentity();
        openLogin(tab);
      }
      showToast(error.message, true);
    });
  }
}

function refreshCurrent() {
  tabInfo[state.tab].loader().catch((error) => {
    if (error.status === 401) {
      state.token = '';
      state.username = '';
      updateIdentity();
      openLogin(state.tab);
    }
    showToast(error.message, true);
  });
}

async function checkHealth() {
  const element = $('#health');
  try {
    await api('/api/health');
    element.textContent = '服务在线';
    element.className = 'badge success';
  } catch {
    element.textContent = '服务离线';
    element.className = 'badge danger';
  }
}

function kpi(label, value, hint) {
  return `<div class="kpi"><span>${escapeHtml(label)}</span><strong>${escapeHtml(value)}</strong><small>${escapeHtml(hint)}</small></div>`;
}

function settled(result, fallback) {
  return result.status === 'fulfilled' && result.value !== null ? result.value : fallback;
}

async function loadOverview() {
  const results = await Promise.allSettled([
    api(`${API}/data-ingestion/sources`),
    api(`${API}/data-ingestion/tasks`),
    api(`${API}/student-profile/students`),
    api(`${API}/academic-alerts/dashboard`),
    api(`${API}/data-governance/quality-report`),
    api('/api/modules'),
  ]);
  const sources = settled(results[0], []);
  const tasks = settled(results[1], []);
  const students = settled(results[2], []);
  const dashboard = settled(results[3], {});
  const quality = unwrapM2(settled(results[4], {})) || {};
  const modules = settled(results[5], []);
  const authed = Boolean(state.token);

  $('#overview-kpis').innerHTML = [
    kpi('数据源', authed ? sources.length : '—', 'M1 · MySQL'),
    kpi('导入任务', authed ? tasks.length : '—', 'M1 · 可执行'),
    kpi('学生', authed ? students.length : '—', 'M3 · 画像'),
    kpi('待处理预警', authed ? (dashboard.active_alerts ?? '—') : '—', 'M4 · 落库'),
    kpi('质量得分', authed ? (quality.quality_score ?? '—') : '—', 'M2 · 落库'),
  ].join('');

  const hint = $('#overview-login-hint');
  if (hint) hint.hidden = authed;

  const tabByModule = {
    'data-ingestion': 'ingestion',
    'data-governance': 'governance',
    'student-profile': 'profile',
    'academic-alerts': 'alerts',
  };
  const linkByModule = {
    'career-recommendation': '/career',
    'data-services': '/data-services',
  };
  $('#module-list').innerHTML = modules.length
    ? modules.map((module) => {
      const count = Array.isArray(module.routes) ? module.routes.length : 0;
      const action = tabByModule[module.id]
        ? `<button class="link" data-goto="${tabByModule[module.id]}">打开</button>`
        : linkByModule[module.id]
          ? `<a href="${linkByModule[module.id]}">打开 ↗</a>`
          : '';
      return `<div class="module-row">
        <div><div class="name">${escapeHtml(module.name)}</div>
        <div class="meta">${escapeHtml(module.id)} · ${count} 条路由</div></div>
        ${action}
      </div>`;
    }).join('')
    : empty('读取模块列表失败。');
}

async function loadIngestion() {
  const [sources, tasks] = await Promise.all([
    api(`${API}/data-ingestion/sources`),
    api(`${API}/data-ingestion/tasks`),
  ]);
  state.sources = Array.isArray(sources) ? sources : [];
  state.tasks = Array.isArray(tasks) ? tasks : [];
  renderSources();
  renderTasks();
}

function renderSources() {
  const container = $('#source-list');
  if (!state.sources.length) {
    container.innerHTML = empty('还没有数据源，点右上角“新增数据源”。');
    return;
  }
  container.innerHTML = `<table><thead><tr>
    <th>ID</th><th>名称</th><th>类型</th><th>接入方式</th><th>责任人</th><th>创建时间</th>
  </tr></thead><tbody>${state.sources.map((source) => `<tr>
    <td><code>${source.id}</code></td>
    <td><b>${escapeHtml(source.name)}</b><div class="hint">${escapeHtml(source.description || '')}</div></td>
    <td>${badge(kindLabel(source.kind), 'info')}</td>
    <td>${escapeHtml(modeLabel(source.access_mode))}</td>
    <td>${escapeHtml(source.owner || '—')}</td>
    <td>${escapeHtml(source.created_at || '—')}</td>
  </tr>`).join('')}</tbody></table>`;
}

function renderTasks() {
  const container = $('#task-list');
  if (!state.tasks.length) {
    container.innerHTML = empty('还没有导入任务。先运行 demo 脚本或通过接口创建任务。');
    return;
  }
  container.innerHTML = `<table><thead><tr>
    <th>ID</th><th>任务</th><th>数据源</th><th>模式</th><th>状态</th><th>操作</th>
  </tr></thead><tbody>${state.tasks.map((task) => `<tr>
    <td><code>${task.id}</code></td>
    <td><b>${escapeHtml(task.name)}</b><div class="hint">校验规则 ${escapeHtml(JSON.stringify(task.validation_rules || {}))}</div></td>
    <td>${task.data_source_id}</td>
    <td>${escapeHtml(modeLabel(task.import_mode))}</td>
    <td>${badge(task.status, statusTone(task.status))}</td>
    <td class="actions"><button class="btn ghost small" data-run-task="${task.id}">执行</button></td>
  </tr>`).join('')}</tbody></table>`;
}

function openRun(taskId) {
  const task = state.tasks.find((item) => String(item.id) === String(taskId));
  if (!task) return;
  state.runTaskId = task.id;
  $('#run-panel').hidden = false;
  $('#run-title').textContent = `执行导入 · ${task.name}`;
  $('#run-records').value = JSON.stringify([
    { xh: '2023001', cj: '92' },
    { xh: '', cj: '50' },
  ], null, 2);
  $('#run-result').innerHTML = '';
  $('#run-panel').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

function renderRunResult(execution) {
  const run = execution.run || {};
  const failures = Array.isArray(execution.failures) ? execution.failures : [];
  $('#run-result').innerHTML = `<div class="result-box">
    <h4>执行结果 ${badge(run.status || 'unknown', statusTone(run.status))}</h4>
    <div class="result-grid">
      <div><span>总记录</span><strong>${run.total_count ?? 0}</strong></div>
      <div><span>成功</span><strong>${run.success_count ?? 0}</strong></div>
      <div><span>失败</span><strong>${run.failure_count ?? 0}</strong></div>
      <div><span>Run ID</span><strong>${run.id ?? '—'}</strong></div>
    </div>
    ${failures.length ? `<h4>失败明细</h4><table><thead><tr><th>行号</th><th>原因</th><th>原始数据</th></tr></thead><tbody>${failures.map((record) => `<tr>
      <td>${record.source_row_no}</td>
      <td>${escapeHtml(record.error_reason || '')}</td>
      <td><code>${escapeHtml(JSON.stringify(record.payload))}</code></td>
    </tr>`).join('')}</tbody></table>` : '<p class="hint">没有失败记录。</p>'}
  </div>`;
}

async function submitSourceForm(event) {
  event.preventDefault();
  const form = event.currentTarget;
  const data = Object.fromEntries(new FormData(form).entries());
  try {
    await api(`${API}/data-ingestion/sources`, { method: 'POST', body: data });
    showToast('数据源已创建。');
    form.reset();
    form.hidden = true;
    await loadIngestion();
  } catch (error) {
    showToast(error.message, true);
  }
}

async function submitRun(event) {
  event.preventDefault();
  if (!state.runTaskId) return;
  try {
    const records = JSON.parse($('#run-records').value);
    if (!Array.isArray(records)) throw new Error('记录必须是 JSON 数组');
    const execution = await api(`${API}/data-ingestion/tasks/${state.runTaskId}/run`, {
      method: 'POST',
      body: { records, idempotency_key: `ui-${Date.now()}` },
    });
    renderRunResult(execution);
    showToast('导入完成。');
    await loadIngestion();
  } catch (error) {
    showToast(error.message, true);
  }
}

async function loadGovernance() {
  const [reportRaw, rulesRaw, mastersRaw] = await Promise.all([
    api(`${API}/data-governance/quality-report`),
    api(`${API}/data-governance/quality-rules`),
    api(`${API}/data-governance/student-master`),
  ]);
  state.governanceReport = unwrapM2(reportRaw) || {};
  state.ruleRows = unwrapM2(rulesRaw) || [];
  state.masterRows = unwrapM2(mastersRaw) || [];
  renderGovernance();
}

function renderGovernance() {
  const report = state.governanceReport || {};
  $('#quality-kpis').innerHTML = [
    kpi('总记录', report.total_records ?? '—', '质量报告'),
    kpi('已清洗', report.cleaned_records ?? '—', '质量报告'),
    kpi('脏数据', report.dirty_records ?? '—', '质量报告'),
    kpi('质量得分', report.quality_score ?? '—', 'M2 · 落库'),
    kpi('主数据覆盖', report.main_data_coverage != null ? `${report.main_data_coverage}%` : '—', 'M2 · 落库'),
  ].join('');
  renderQualityRules();
  renderMasterList();
}

function renderQualityRules() {
  const list = state.ruleRows || [];
  $('#rule-list').innerHTML = list.length
    ? `<table><thead><tr><th>ID</th><th>规则</th><th>类型</th><th>字段</th><th>级别</th><th>状态</th><th></th></tr></thead><tbody>${list.map((rule) => `<tr class="${rule.enabled ? '' : 'row-disabled'}">
        <td>${rule.rule_id}</td><td>${escapeHtml(rule.rule_name)}</td>
        <td>${escapeHtml(ruleTypeLabel(rule.rule_type))}</td><td><code>${escapeHtml(rule.target_field)}</code></td>
        <td>${badge(qualityLevelLabel(rule.level), qualityLevelTone(rule.level))}</td>
        <td>${badge(rule.enabled ? '启用' : '停用', rule.enabled ? 'success' : 'neutral')}</td>
        <td class="actions"><button class="btn ghost small" data-rule-toggle="${rule.rule_id}" data-enabled="${rule.enabled ? 1 : 0}">${rule.enabled ? '停用' : '启用'}</button></td>
      </tr>`).join('')}</tbody></table>`
    : empty('暂无质量规则。');
}

function renderMasterList() {
  const paged = pageSlice(state.masterRows || [], 'master');
  $('#master-list').innerHTML = paged.items.length
    ? `<table class="master-table"><thead><tr><th>学生</th><th>专业</th><th>班级</th><th>状态</th></tr></thead><tbody>${paged.items.map((row) => {
        const id = escapeHtml(row.student_id);
        const no = row.student_no ? ` · 学号 ${escapeHtml(row.student_no)}` : '';
        const status = row.status === 'active' ? '在读' : (row.status || '未知');
        return `<tr class="row-link" data-master-id="${id}">
        <td><b>${escapeHtml(row.name)}</b><div class="hint">${id}${no}</div></td>
        <td>${escapeHtml(row.major || '—')}</td>
        <td>${escapeHtml(row.class_name || row.class || '—')}</td>
        <td>${badge(status, row.status === 'active' ? 'success' : 'neutral')}</td>
      </tr>`;
      }).join('')}</tbody></table>`
    : empty('暂无学生主数据。');
  renderPager('#master-pager', paged, 'master');
}

async function toggleQualityRule(id, nextEnabled) {
  try {
    await api(`${API}/data-governance/quality-rules/${encodeURIComponent(id)}`, {
      method: 'PATCH',
      body: { enabled: nextEnabled },
    });
    showToast(nextEnabled ? '质量规则已启用。' : '质量规则已停用。');
    await loadGovernance();
  } catch (error) {
    showToast(error.message, true);
  }
}

async function submitRuleForm(event) {
  event.preventDefault();
  const form = event.currentTarget;
  const data = Object.fromEntries(new FormData(form).entries());
  try {
    await api(`${API}/data-governance/quality-rules`, { method: 'POST', body: data });
    showToast('质量规则已保存到数据库。');
    form.reset();
    await loadGovernance();
  } catch (error) {
    showToast(error.message, true);
  }
}

async function lookupMaster(event) {
  event.preventDefault();
  await lookupMasterById($('#master-id').value.trim());
}

async function lookupMasterById(id) {
  if (!id) {
    $('#master-result').innerHTML = empty('请输入主数据 ID。');
    return;
  }
  try {
    const master = unwrapM2(await api(`${API}/data-governance/student-master/${encodeURIComponent(id)}`));
    $('#master-result').innerHTML = `<div class="result-box"><h4>学生主数据</h4><table><tbody>
      ${Object.entries(master || {}).map(([key, value]) => `<tr><th>${escapeHtml(key)}</th><td>${escapeHtml(value)}</td></tr>`).join('')}
    </tbody></table></div>`;
  } catch (error) {
    $('#master-result').innerHTML = empty(error.message);
  }
}

async function triggerRecalc() {
  const term = (($('#recalc-term') && $('#recalc-term').value) || '').trim();
  try {
    const result = unwrapM2(await api(`${API}/data-governance/recalculate`, {
      method: 'POST',
      body: term ? { term } : {},
    })) || {};
    const box = $('#recalc-result');
    box.hidden = false;
    const students = Array.isArray(result.results) ? result.results : [];
    const rows = students
      .flatMap((item) => Object.entries(item.metrics || {}).map(([code, value]) =>
        `<tr><td>${escapeHtml(item.student_id)}</td><td><code>${escapeHtml(code)}</code></td><td>${value}</td></tr>`))
      .join('');
    box.innerHTML = `<div class="card-head"><h3>指标重算结果</h3>${badge(result.status || 'finished', 'success')}</div>
      <div class="result-grid">
        <div><span>任务 ID</span><strong>${escapeHtml(result.task_id || '—')}</strong></div>
        <div><span>学期</span><strong>${escapeHtml(result.term || '—')}</strong></div>
        <div><span>学生数</span><strong>${result.student_count ?? 0}</strong></div>
        <div><span>指标数</span><strong>${result.metric_count ?? 0}</strong></div>
      </div>
      ${rows
        ? `<table><thead><tr><th>学生</th><th>指标</th><th>数值</th></tr></thead><tbody>${rows}</tbody></table>`
        : '<p class="hint">没有可用的原始记录，先在“数据接入”页导入数据。</p>'}
      <p class="hint">结果已写入 M3 的 sp_metric_value，可在“学生画像”页查看。</p>`;
    showToast('指标重算完成并已落库。');
  } catch (error) {
    showToast(error.message, true);
  }
}

async function loadProfile() {
  const students = await api(`${API}/student-profile/students`);
  state.students = Array.isArray(students) ? students : [];
  renderStudents();
  if (!state.students.length) {
    $('#profile-detail').innerHTML = empty('还没有学生数据。先运行 py -3 scripts/seed_demo.py <端口> 导入演示数据。');
    return;
  }
  const selected = state.selectedStudent && state.students.some((student) => student.student_id === state.selectedStudent)
    ? state.selectedStudent
    : state.students[0].student_id;
  await selectStudent(selected);
}

function renderStudents() {
  const container = $('#student-list');
  if (!state.students.length) {
    container.innerHTML = empty('暂无学生。');
    return;
  }
  container.innerHTML = state.students.map((student) => `<button class="student-row ${student.student_id === state.selectedStudent ? 'active' : ''}" data-student-id="${escapeHtml(student.student_id)}">
    <span><b>${escapeHtml(student.name)}</b><small>${escapeHtml(student.student_id)} · ${escapeHtml(student.major || '未设置专业')}</small></span>
    <span class="badge info">${escapeHtml(student.class_name || '—')}</span>
  </button>`).join('');
}

async function selectStudent(studentId) {
  state.selectedStudent = studentId;
  renderStudents();
  const term = $('#term').value.trim();
  try {
    const profile = await api(`${API}/student-profile/students/${encodeURIComponent(studentId)}${term ? `?term=${encodeURIComponent(term)}` : ''}`);
    renderProfile(profile);
  } catch (error) {
    $('#profile-detail').innerHTML = empty(error.message);
  }
}

function renderProfile(profile) {
  const student = profile.student || {};
  const metrics = Array.isArray(profile.metrics) ? profile.metrics : [];
  const tags = Array.isArray(profile.tags) ? profile.tags : [];
  const term = profile.term || $('#term').value.trim();
  const reportPath = `/api/modules/student-profile/students/${encodeURIComponent(student.student_id)}/report${term ? `?term=${encodeURIComponent(term)}` : ''}`;

  $('#profile-detail').innerHTML = `<div class="profile-head">
    <div><h2>${escapeHtml(student.name || student.student_id)}</h2>
      <div class="profile-meta">${escapeHtml(student.student_id)} · ${escapeHtml(student.major || '未设置专业')} · ${escapeHtml(student.class_name || '未设置班级')}</div>
      <div class="profile-meta">学期：${escapeHtml(term || '未指定')}</div>
    </div>
    <a class="btn ghost small" href="${reportPath}" target="_blank" rel="noopener">打开画像报告</a>
  </div>
  <div class="card-head"><h3>指标</h3>${badge(`${metrics.length} 项`, 'info')}</div>
  ${metrics.length ? `<table><thead><tr><th>指标</th><th>数值</th><th>单位</th><th>来源</th><th>计算时间</th></tr></thead><tbody>${metrics.map((metric) => `<tr>
    <td><b>${escapeHtml(metric.metric_code)}</b></td><td>${escapeHtml(metric.value)}</td>
    <td>${escapeHtml(metric.unit || '—')}</td><td>${escapeHtml(metric.source || '—')}</td>
    <td>${escapeHtml(metric.calculated_at || '—')}</td>
  </tr>`).join('')}</tbody></table>` : empty('该学期暂无指标。')}
  <div class="card-head" style="margin-top:18px"><h3>标签</h3>${badge(`${tags.length} 个`, 'purple')}</div>
  ${tags.length ? `<div class="tag-list">${tags.map((tag) => `<span class="tag"><strong>${escapeHtml(tag.name || tag.code)}</strong> · ${escapeHtml(tag.dimension || '未分类')}
    <details class="evidence"><summary>依据</summary><pre>${escapeHtml(JSON.stringify(tag.evidence || {}, null, 2))}</pre></details>
  </span>`).join('')}</div>` : empty('该学期暂无标签，点“批量重算”生成。')}
  ${profile.snapshot ? `<p class="hint" style="margin-top:14px">画像快照生成时间：${escapeHtml(profile.snapshot.generated_at || '—')}</p>` : ''}`;
}

async function batchRecalculate() {
  const term = $('#term').value.trim();
  if (!term) {
    showToast('请先填写学期，例如 2026-fall。', true);
    return;
  }
  try {
    const result = await api(`${API}/student-profile/recalculate/batch`, {
      method: 'POST',
      body: { term },
    });
    const box = $('#batch-result');
    box.hidden = false;
    box.textContent = `批量重算完成：学期 ${result.term}，共 ${result.count} 名学生。`;
    await loadProfile();
    showToast('批量重算完成。');
  } catch (error) {
    showToast(error.message, true);
  }
}

function alertQuery() {
  const params = new URLSearchParams();
  if (state.alertFilter.status) params.set('status', state.alertFilter.status);
  if (state.alertFilter.level) params.set('level', state.alertFilter.level);
  if (state.alertFilter.className) params.set('class', state.alertFilter.className);
  const query = params.toString();
  return query ? `?${query}` : '';
}

async function loadAlerts() {
  const [dashboard, rules, alerts] = await Promise.all([
    api(`${API}/academic-alerts/dashboard`),
    api(`${API}/academic-alerts/rules`),
    api(`${API}/academic-alerts/alerts${alertQuery()}`),
  ]);
  state.alertDashboard = dashboard || {};
  state.alertRules = Array.isArray(rules.rules) ? rules.rules : [];
  state.alerts = Array.isArray(alerts.alerts) ? alerts.alerts : [];
  state.alertTotal = alerts.total ?? state.alerts.length;
  state.alertOverdue = alerts.overdue ?? 0;
  state.pages.alerts = 1;
  renderAlerts();
}

function renderAlerts() {
  const dashboard = state.alertDashboard || {};
  $('#alert-kpis').innerHTML = [
    kpi('学生数', dashboard.student_count ?? '—', 'M4 · 落库'),
    kpi('平均成绩', dashboard.avg_score != null ? Number(dashboard.avg_score).toFixed(1) : '—', '看板'),
    kpi('不及格人数', dashboard.failing_students ?? '—', '看板'),
    kpi('活跃预警', dashboard.active_alerts ?? '—', '看板'),
    kpi('超时预警', dashboard.overdue_alerts ?? '—', '看板'),
  ].join('');

  const rules = state.alertRules || [];
  $('#alert-rules').innerHTML = rules.length
    ? `<table><thead><tr><th>ID</th><th>规则</th><th>条件</th><th>等级</th><th>状态</th><th></th></tr></thead><tbody>${rules.map((rule) => `<tr class="${rule.enabled ? '' : 'row-disabled'}">
      <td>${escapeHtml(rule.id)}</td><td>${escapeHtml(rule.name)}</td>
      <td><code>${escapeHtml(rule.metric)} ${escapeHtml(rule.operator)} ${escapeHtml(rule.threshold)}</code></td>
      <td>${badge(rule.level, levelTone(rule.level))}</td>
      <td>${badge(rule.enabled ? '启用' : '停用', rule.enabled ? 'success' : 'neutral')}</td>
      <td class="actions"><button class="btn ghost small" data-alert-rule-toggle="${escapeHtml(rule.id)}" data-enabled="${rule.enabled ? 1 : 0}">${rule.enabled ? '停用' : '启用'}</button></td>
    </tr>`).join('')}</tbody></table>`
    : empty('暂无预警规则。');

  $('#alert-count').textContent = `${state.alertTotal} 条 · 超时 ${state.alertOverdue}`;
  const paged = pageSlice(state.alerts, 'alerts');
  $('#alert-list').innerHTML = paged.items.length
    ? paged.items.map((alert) => {
        const active = alert.status === 'open' || alert.status === 'claimed';
        const term = alert.metric_term ? ` · 学期 ${escapeHtml(alert.metric_term)}` : '';
        return `<div class="alert-item">
        <div class="alert-line"><b>${escapeHtml(alert.student_name || alert.student_id)}</b>
          ${badge(alert.level, levelTone(alert.level))}
          ${badge(alert.status, statusTone(alert.status))}
          ${alert.overdue ? badge('超时', 'danger') : ''}
        </div>
        <p class="alert-meta">${escapeHtml(alert.class_name || '—')} · 学号 ${escapeHtml(alert.student_id)} · 规则 ${escapeHtml(alert.rule_id)} · 指标值 ${escapeHtml(alert.metric_value)}${term} · 认领人 ${escapeHtml(alert.assignee || '未认领')}</p>
        <div class="alert-actions">
          ${alert.status === 'open' ? `<button class="btn ghost small" data-claim="${alert.id}">认领</button>` : ''}
          ${active ? `<button class="btn small" data-handle="${alert.id}">处置</button>` : ''}
          <button class="btn ghost small" data-history="${alert.id}">干预记录</button>
        </div>
      </div>`;
      }).join('')
    : empty('当前筛选条件下没有预警。');
  renderPager('#alert-pager', paged, 'alerts');
}

async function toggleAlertRule(id, nextEnabled) {
  try {
    await api(`${API}/academic-alerts/rules/${encodeURIComponent(id)}`, {
      method: 'PATCH',
      body: { enabled: nextEnabled },
    });
    showToast(nextEnabled ? '预警规则已启用。' : '预警规则已停用。');
    await loadAlerts();
  } catch (error) {
    showToast(error.message, true);
  }
}

function applyAlertFilters() {
  state.alertFilter = {
    status: $('#alert-status').value,
    level: $('#alert-level').value,
    className: $('#alert-class').value.trim(),
  };
  loadAlerts().catch((error) => showToast(error.message, true));
}

function resetAlertFilters() {
  $('#alert-status').value = 'open';
  $('#alert-level').value = '';
  $('#alert-class').value = '';
  applyAlertFilters();
}

async function generateAlerts() {
  try {
    const result = await api(`${API}/academic-alerts/alerts/generate`, { method: 'POST' });
    showToast(`生成 ${result.generated ?? 0} 条，跳过 ${result.skipped ?? 0} 条。`);
    await loadAlerts();
  } catch (error) {
    showToast(error.message, true);
  }
}

async function claimAlert(id) {
  const assignee = window.prompt('认领人姓名', '王老师');
  if (!assignee) return;
  try {
    await api(`${API}/academic-alerts/alerts/${id}/claim`, { method: 'POST', body: { assignee } });
    showToast('预警已认领。');
    await loadAlerts();
  } catch (error) {
    showToast(error.message, true);
  }
}

function openHandleDialog(id) {
  const alert = state.alerts.find((item) => String(item.id) === String(id));
  state.alertId = id;
  $('#dialog-alert-id').value = id;
  $('#dialog-title').textContent = `处置预警 · ${alert?.student_name || id}`;
  $('#dialog-action').value = 'follow';
  $('#dialog-measures').value = '';
  $('#dialog-note').value = '';
  $('#dialog-by').value = alert?.assignee || '王老师';
  $('#alert-dialog').showModal();
}

async function submitHandle(event) {
  event.preventDefault();
  const id = state.alertId;
  if (!id) return;
  const body = {
    action: $('#dialog-action').value,
    measures: $('#dialog-measures').value.trim(),
    note: $('#dialog-note').value.trim(),
    by: $('#dialog-by').value.trim(),
  };
  try {
    await api(`${API}/academic-alerts/alerts/${id}/handle`, { method: 'POST', body });
    $('#alert-dialog').close();
    showToast('预警已处置。');
    await loadAlerts();
  } catch (error) {
    showToast(error.message, true);
  }
}

async function showHistory(id) {
  try {
    const data = await api(`${API}/academic-alerts/alerts/${id}/interventions`);
    const box = $('#alert-history');
    box.hidden = false;
    box.innerHTML = `<div class="card-head"><h3>干预记录 · 预警 ${escapeHtml(id)}</h3>
      <button class="btn ghost small" data-close-history>关闭</button></div>
      <p class="hint">状态：${escapeHtml(data.status)} · ${data.overdue ? '已超时' : '未超时'}</p>
      ${Array.isArray(data.records) && data.records.length ? `<table><thead><tr><th>时间</th><th>动作</th><th>措施</th><th>备注</th><th>处理人</th></tr></thead><tbody>${data.records.map((record) => `<tr>
        <td>${escapeHtml(record.at)}</td><td>${escapeHtml(record.action)}</td>
        <td>${escapeHtml(record.measures || '—')}</td><td>${escapeHtml(record.note || '—')}</td>
        <td>${escapeHtml(record.by || '—')}</td>
      </tr>`).join('')}</tbody></table>` : empty('暂无干预记录。')}`;
    box.scrollIntoView({ behavior: 'smooth', block: 'start' });
  } catch (error) {
    showToast(error.message, true);
  }
}

function bindEvents() {
  $$('.nav-item').forEach((button) => {
    button.addEventListener('click', () => switchTab(button.dataset.tab));
  });
  $('#refresh').addEventListener('click', refreshCurrent);
  $('#login-button').addEventListener('click', () => openLogin());
  $('#logout-button').addEventListener('click', logout);
  $('#login-form').addEventListener('submit', submitLogin);
  $('#login-cancel').addEventListener('click', () => $('#login-dialog').close());
  $$('[data-refresh]').forEach((button) => {
    button.addEventListener('click', () => {
      const tab = button.dataset.refresh;
      tabInfo[tab].loader().catch((error) => showToast(error.message, true));
    });
  });
  $('#module-list').addEventListener('click', (event) => {
    const button = event.target.closest('[data-goto]');
    if (button) switchTab(button.dataset.goto);
  });

  $('#toggle-source-form').addEventListener('click', () => { $('#source-form').hidden = false; });
  $('#cancel-source').addEventListener('click', () => { $('#source-form').hidden = true; });
  $('#source-form').addEventListener('submit', submitSourceForm);
  $('#task-list').addEventListener('click', (event) => {
    const button = event.target.closest('[data-run-task]');
    if (button) openRun(button.dataset.runTask);
  });
  $('#close-run').addEventListener('click', () => { $('#run-panel').hidden = true; });
  $('#run-submit').addEventListener('click', submitRun);

  $('#rule-form').addEventListener('submit', submitRuleForm);
  $('#master-form').addEventListener('submit', lookupMaster);
  $('#recalc-metrics').addEventListener('click', triggerRecalc);
  $('#rule-list').addEventListener('click', (event) => {
    const button = event.target.closest('[data-rule-toggle]');
    if (button) toggleQualityRule(button.dataset.ruleToggle, button.dataset.enabled !== '1');
  });
  $('#master-list').addEventListener('click', (event) => {
    const row = event.target.closest('[data-master-id]');
    if (!row) return;
    $('#master-id').value = row.dataset.masterId;
    lookupMasterById(row.dataset.masterId).catch((error) => showToast(error.message, true));
  });
  $('#master-pager').addEventListener('click', (event) => {
    const button = event.target.closest('[data-page]');
    if (!button) return;
    state.pages.master = Number(button.dataset.page);
    renderMasterList();
  });

  $('#student-list').addEventListener('click', (event) => {
    const button = event.target.closest('[data-student-id]');
    if (button) selectStudent(button.dataset.studentId).catch((error) => showToast(error.message, true));
  });
  $('#term').addEventListener('change', () => {
    if (state.selectedStudent) selectStudent(state.selectedStudent).catch((error) => showToast(error.message, true));
  });
  $('#batch-recalc').addEventListener('click', batchRecalculate);

  $('#generate-alerts').addEventListener('click', generateAlerts);
  ['#alert-status', '#alert-level'].forEach((selector) => {
    $(selector).addEventListener('change', applyAlertFilters);
  });
  $('#alert-class').addEventListener('change', applyAlertFilters);
  $('#alert-filter-reset').addEventListener('click', resetAlertFilters);
  $('#alert-rules').addEventListener('click', (event) => {
    const button = event.target.closest('[data-alert-rule-toggle]');
    if (button) toggleAlertRule(button.dataset.alertRuleToggle, button.dataset.enabled !== '1');
  });
  $('#alert-pager').addEventListener('click', (event) => {
    const button = event.target.closest('[data-page]');
    if (!button) return;
    state.pages.alerts = Number(button.dataset.page);
    renderAlerts();
  });
  $('#alert-list').addEventListener('click', (event) => {
    const claim = event.target.closest('[data-claim]');
    if (claim) { claimAlert(claim.dataset.claim); return; }
    const handle = event.target.closest('[data-handle]');
    if (handle) { openHandleDialog(handle.dataset.handle); return; }
    const history = event.target.closest('[data-history]');
    if (history) showHistory(history.dataset.history);
  });
  $('#alert-form').addEventListener('submit', submitHandle);
  $('#dialog-cancel').addEventListener('click', () => $('#alert-dialog').close());
  $('#alert-history').addEventListener('click', (event) => {
    if (event.target.closest('[data-close-history]')) $('#alert-history').hidden = true;
  });
}

bindEvents();
updateIdentity();
checkHealth();
switchTab('overview');
