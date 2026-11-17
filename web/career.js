const prefix = "/api/modules/career-recommendation";
const jobs = document.querySelector("#jobs");
const jobForm = document.querySelector("#job-form");
const strategyForm = document.querySelector("#strategy-form");
const requirements = document.querySelector("#requirements");
const jobFields = document.querySelector("#job-fields");
const strategyFields = document.querySelector("#strategy-fields");
const jobsMessage = document.querySelector("#jobs-message");
const strategyMessage = document.querySelector("#strategy-message");
const jobTypes = { campus: "校招", internship: "实习" };
const statusLabels = { draft: "草稿", published: "已发布", withdrawn: "已下架" };
let editing = null;
let jobBusy = false;
let demo = false;
let token = "";

async function request(path, method = "GET", data, extraHeaders = {}) {
  const headers = { "Content-Type": "application/json", ...extraHeaders };
  if (token) headers.Authorization = "Bearer " + token;
  const response = await fetch(path, {
    method,
    headers,
    body: data === undefined ? undefined : JSON.stringify(data),
  });
  const body = await response.json();
  if (!response.ok) {
    const error = new Error(body.message ?? body.error);
    error.status = response.status;
    throw error;
  }
  return body;
}

function api(path, method = "GET", data, student) {
  const headers = demo ? { "X-Career-Demo": "1" } : {};
  if (demo && student) headers["X-Career-Demo-Student"] = student;
  return request(prefix + path, method, data, headers);
}

function addRequirement(values) {
  const template = document.querySelector("#requirement-template");
  const row = template.content.firstElementChild.cloneNode(true);
  if (values) {
    for (const name of ["competency_code", "threshold", "weight"]) {
      row.querySelector(`[name="${name}"]`).value = values[name];
    }
  }
  row.querySelector("button").addEventListener("click", () => {
    if (requirements.children.length === 1) {
      jobsMessage.textContent = "岗位至少保留一项能力要求";
      return;
    }
    row.remove();
  });
  requirements.append(row);
}

function editJob(job) {
  editing = job?.id ?? null;
  jobForm.reset();
  requirements.replaceChildren();
  document.querySelector("#job-heading").textContent = job ? `编辑岗位 #${job.id}` : "新增岗位";
  document.querySelector("#save-job").textContent = job ? "保存修改" : "保存为草稿";
  document.querySelector("#cancel-edit").hidden = !job;
  if (!job) {
    addRequirement();
    return;
  }
  for (const name of ["title", "industry", "job_type", "description"]) {
    jobForm.elements[name].value = job[name];
  }
  job.requirements.forEach(addRequirement);
}

function action(label, callback) {
  const button = document.createElement("button");
  button.type = "button";
  button.textContent = label;
  button.addEventListener("click", callback);
  return button;
}

async function loadJobs() {
  const catalog = await api("/jobs");
  jobs.replaceChildren();
  if (!catalog.length) jobs.textContent = "暂无岗位，请先新增";
  for (const job of catalog) {
    const card = document.createElement("article");
    const title = document.createElement("h3");
    title.textContent = job.title;
    const meta = document.createElement("p");
    meta.textContent = `${job.industry} · ${jobTypes[job.job_type]} · ${statusLabels[job.status]} · v${job.version}`;
    const description = document.createElement("p");
    description.textContent = job.description;
    const details = document.createElement("ul");
    for (const requirement of job.requirements) {
      const item = document.createElement("li");
      item.textContent = `${requirement.competency_code} · 达标分 ${requirement.threshold} · 权重 ${requirement.weight}`;
      details.append(item);
    }
    const status = job.status === "published" ? "withdrawn" : "published";
    const label = { draft: "发布", published: "下架", withdrawn: "重新发布" }[job.status];
    const buttons = document.createElement("div");
    buttons.className = "career-actions";
    buttons.append(action("编辑", () => {
      if (!jobBusy) {
        editJob(job);
        jobsMessage.textContent = "";
        jobForm.elements.title.focus();
      }
    }), action(label, () => updateJobs(async () => {
      await api(`/jobs/${job.id}/status`, "PUT", { status });
      jobsMessage.textContent = `岗位已${label}`;
    })));
    card.append(title, meta, description, details, buttons);
    jobs.append(card);
  }
}

async function updateJobs(change) {
  if (jobBusy) return;
  jobBusy = true;
  jobFields.disabled = true;
  try {
    await change();
    await loadJobs();
  } catch (error) {
    jobsMessage.textContent = error.message;
  } finally {
    jobBusy = false;
    jobFields.disabled = false;
  }
}

function syncAlgorithm(useDefaults = false) {
  const { algorithm, pass_weight, proximity_weight } = strategyForm.elements;
  const fixed = algorithm.value === "weighted_pass";
  pass_weight.readOnly = proximity_weight.readOnly = fixed;
  if (fixed || useDefaults) {
    pass_weight.value = fixed ? 100 : 60;
    proximity_weight.value = fixed ? 0 : 40;
  }
}

async function loadStrategy() {
  try {
    const strategy = await api("/strategy");
    for (const name of ["algorithm", "min_score", "pass_weight", "proximity_weight"]) {
      strategyForm.elements[name].value = strategy[name];
    }
    strategyForm.elements.allowed_industries.value = strategy.allowed_industries.join("\n");
    for (const type of strategyForm.querySelectorAll("[name=allowed_job_types]")) {
      type.checked = strategy.allowed_job_types.includes(type.value);
    }
    syncAlgorithm();
    strategyMessage.textContent = `当前生效版本 v${strategy.version}`;
  } catch (error) {
    strategyMessage.textContent = error.status === 409 ? "尚未配置，保存后策略才会生效" : error.message;
    if (error.status !== 409) return;
  }
  strategyFields.disabled = false;
}

document.querySelector("#add-requirement").addEventListener("click", () => {
  if (requirements.children.length < 100) addRequirement();
});
document.querySelector("#cancel-edit").addEventListener("click", () => editJob());
jobForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const form = new FormData(jobForm);
  const job = {
    title: form.get("title"),
    industry: form.get("industry"),
    job_type: form.get("job_type"),
    description: form.get("description"),
    requirements: [...requirements.children].map((row) => ({
      competency_code: row.querySelector("[name=competency_code]").value,
      threshold: row.querySelector("[name=threshold]").valueAsNumber,
      weight: row.querySelector("[name=weight]").valueAsNumber,
    })),
  };
  updateJobs(async () => {
    const saved = await api(editing === null ? "/jobs" : `/jobs/${editing}`,
      editing === null ? "POST" : "PUT", job);
    jobsMessage.textContent = `已保存岗位 #${saved.id}`;
    editJob();
  });
});

strategyForm.elements.algorithm.addEventListener("change", () => syncAlgorithm(true));
strategyForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(strategyForm);
  strategyFields.disabled = true;
  try {
    const saved = await api("/strategy", "PUT", {
      algorithm: form.get("algorithm"),
      min_score: Number(form.get("min_score")),
      pass_weight: Number(form.get("pass_weight")),
      proximity_weight: Number(form.get("proximity_weight")),
      allowed_industries: form.get("allowed_industries").split("\n").map((value) => value.trim()).filter(Boolean),
      allowed_job_types: form.getAll("allowed_job_types"),
    });
    strategyMessage.textContent = `当前生效版本 v${saved.version}`;
  } catch (error) {
    strategyMessage.textContent = error.message;
  } finally {
    strategyFields.disabled = false;
  }
});

const login = document.querySelector("#career-login");
const mode = document.querySelector("#career-mode");
const message = document.querySelector("#career-message");
const logout = document.querySelector("#career-logout");
const outcomesView = document.querySelector("#outcomes-view");
const outcomesForm = document.querySelector("#outcomes-form");
const outcomesMessage = document.querySelector("#outcomes-message");
const outcomesTables = document.querySelector("#outcomes-tables");
const studentView = document.querySelector("#student-view");
const recommendationForm = document.querySelector("#recommendation-form");
const recommendationList = document.querySelector("#recommendation-list");
const recommendationMessage = document.querySelector("#recommendation-message");
const demoStudent = recommendationForm.elements.demo_student;
const securityPrefix = "/api/modules/data-services";
let studentViewVersion = 0;
let outcomesVersion = 0;

function resetRecommendations() {
  studentViewVersion += 1;
  recommendationList.replaceChildren();
  recommendationMessage.textContent = "";
}

function showView(view, permissions = []) {
  login.hidden = demo || !!token;
  logout.hidden = !token;
  studentView.hidden = view !== "student";
  resetRecommendations();
  const managesCatalog = view === "manager" && (demo || permissions.some((code) =>
    code.startsWith("career.jobs.") || code === "career.strategy.write"));
  for (const section of document.querySelectorAll("[data-manager]")) section.hidden = !managesCatalog;
  outcomesView.hidden = view !== "manager" || (!demo && !permissions.includes("career.outcomes.read"));
  outcomesVersion += 1;
  outcomesForm.reset();
  outcomesForm.querySelector("button").disabled = false;
  outcomesMessage.textContent = "";
  outcomesTables.replaceChildren();
  if (managesCatalog) {
    jobFields.disabled = true;
    strategyFields.disabled = true;
    editJob();
    loadJobs().then(() => { jobFields.disabled = false; }).catch((error) => {
      jobsMessage.textContent = error.message;
    });
    loadStrategy();
  }
}

document.querySelector("#demo-manager").addEventListener("click", () => showView("manager"));
document.querySelector("#demo-student").addEventListener("click", () => showView("student"));
demoStudent.addEventListener("change", resetRecommendations);
document.querySelector("#login-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const submit = form.querySelector("button");
  submit.disabled = true;
  try {
    token = (await request(securityPrefix + "/login", "POST", Object.fromEntries(new FormData(form)))).access_token;
    const identity = await request(securityPrefix + "/me");
    message.textContent = "";
    if (identity.student_no && identity.permissions.includes("career.recommendations.read")) {
      mode.textContent = "学生 · 本人推荐";
      showView("student");
    } else if (identity.scope.type === "all" && identity.permissions.some((code) =>
      code.startsWith("career.jobs.") || code === "career.strategy.write" || code === "career.outcomes.read")) {
      mode.textContent = "就业指导中心 · 正式模式";
      showView("manager", identity.permissions);
    } else {
      message.textContent = "当前账号没有职业推荐权限";
      showView("");
    }
  } catch (error) {
    token = "";
    message.textContent = error.message;
    showView("");
  } finally {
    form.reset();
    submit.disabled = false;
  }
});

logout.addEventListener("click", async () => {
  logout.disabled = true;
  try {
    await request(securityPrefix + "/logout", "POST");
  } catch (error) {
    message.textContent = error.message;
  } finally {
    token = "";
    logout.disabled = false;
    mode.textContent = "正式模式 · 请登录";
    showView("");
  }
});

function outcomesTable(title, columns, rows) {
  const table = document.createElement("table");
  table.createCaption().textContent = title;
  const headings = table.createTHead().insertRow();
  for (const label of columns) {
    const cell = document.createElement("th");
    cell.scope = "col";
    cell.textContent = label;
    headings.append(cell);
  }
  const body = table.createTBody();
  for (const values of rows) {
    const row = body.insertRow();
    for (const value of values) row.insertCell().textContent = value;
  }
  return table;
}

outcomesForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const version = outcomesVersion;
  const submit = outcomesForm.querySelector("button");
  const params = new URLSearchParams();
  for (const [name, value] of new FormData(outcomesForm)) {
    if (value !== "") params.set(name, value);
  }
  submit.disabled = true;
  outcomesMessage.textContent = "正在查询…";
  outcomesTables.replaceChildren();
  try {
    const result = await api(params.size ? "/outcomes?" + params : "/outcomes");
    if (version !== outcomesVersion) return;
    const sample = result.source === "demo" ? "演示数据 · " : "";
    if (!result.graduates) {
      outcomesMessage.textContent = sample + "该年份范围暂无毕业生记录";
      return;
    }
    outcomesMessage.textContent = sample + `毕业生 ${result.graduates} 人，已就业 ${result.employed} 人`;
    const labels = {
      employed: "就业", further_study: "升学", abroad: "出国（境）",
      unemployed: "待就业", other: "其他", unknown: "未登记",
    };
    const percentage = (value) => value.toFixed(2) + "%";
    outcomesTables.append(outcomesTable("去向分布", ["去向", "人数", "占比"],
      result.outcome_distribution.map((item) => [labels[item.category], item.count, percentage(item.percent)])));
    if (result.industry_distribution.length) {
      outcomesTables.append(outcomesTable("就业行业分布", ["行业", "人数", "占比"],
        result.industry_distribution.map((item) => [item.industry ?? "未登记行业", item.count, percentage(item.percent)])));
    } else {
      const empty = document.createElement("p");
      empty.textContent = "该年份范围暂无已就业记录";
      outcomesTables.append(empty);
    }
    outcomesTables.append(outcomesTable("历年就业趋势", ["毕业年份", "毕业生", "已就业", "就业占比"],
      result.trend.map((item) => [item.year, item.graduates, item.employed, percentage(item.employment_percent)])));
  } catch (error) {
    if (version === outcomesVersion) outcomesMessage.textContent = error.message;
  } finally {
    if (version === outcomesVersion) submit.disabled = false;
  }
});

function renderReason(container, detail, student, viewVersion) {
  const description = document.createElement("p");
  description.textContent = detail.job.description;
  const items = document.createElement("ul");
  for (const item of detail.criteria) {
    const entry = document.createElement("li");
    const result = item.status === "missing" ? "画像缺失" :
      (item.status === "passed" ? `达标（${item.student_score} 分）` :
        `${item.student_score} 分，差 ${item.gap} 分`);
    entry.textContent = `${item.competency_code} · 达标线 ${item.threshold} · 权重 ${item.weight} · ${result} · 贡献 ${item.contribution.toFixed(2)} 分`;
    items.append(entry);
  }
  const form = document.querySelector("#feedback-template").content.firstElementChild.cloneNode(true);
  const status = form.querySelector("[role=status]");
  if (detail.feedback) {
    form.elements.choice.value = detail.feedback.choice;
    form.elements.reason.value = detail.feedback.reason;
    status.textContent = "已保存反馈";
  }
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (viewVersion !== studentViewVersion) return;
    const submit = form.querySelector("button");
    submit.disabled = true;
    try {
      await api(`/recommendations/${detail.id}/feedback`, "PUT", Object.fromEntries(new FormData(form)), student);
      status.textContent = "已保存反馈";
    } catch (error) {
      status.textContent = error.message;
    } finally {
      submit.disabled = false;
    }
  });
  container.replaceChildren(description, items, form);
}

function renderRecommendation(item, student, viewVersion) {
  const card = document.createElement("article");
  const title = document.createElement("h3");
  title.textContent = item.title;
  const summary = document.createElement("p");
  summary.textContent = `${item.industry} · ${jobTypes[item.job_type]} · ${item.score.toFixed(2)} 分`;
  const details = document.createElement("details");
  const toggle = document.createElement("summary");
  toggle.textContent = "查看匹配理由与反馈";
  const content = document.createElement("div");
  details.append(toggle, content);
  let loading = false;
  let loaded = false;
  details.addEventListener("toggle", async () => {
    if (!details.open || loading || loaded || viewVersion !== studentViewVersion) return;
    loading = true;
    content.textContent = "正在读取匹配理由…";
    try {
      const detail = await api(`/recommendations/${item.id}/reason`, "GET", undefined, student);
      if (viewVersion !== studentViewVersion) return;
      renderReason(content, detail, student, viewVersion);
      loaded = true;
    } catch (error) {
      content.textContent = error.message;
    } finally {
      loading = false;
    }
  });
  card.append(title, summary, details);
  return card;
}

recommendationForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  resetRecommendations();
  const viewVersion = studentViewVersion;
  const filter = {};
  for (const field of ["term", "industry"]) {
    const value = recommendationForm.elements[field].value.trim();
    if (value) filter[field] = value;
  }
  const min = recommendationForm.elements.min_score;
  if (min.value !== "") filter.min_score = min.valueAsNumber;
  const student = demoStudent.value;
  recommendationMessage.textContent = "正在查询…";
  try {
    const result = await api("/recommendations/query", "POST", filter, student);
    if (viewVersion !== studentViewVersion) return;
    recommendationMessage.textContent = `画像周期 ${result.term} · ${result.items.length} 个匹配岗位`;
    recommendationList.replaceChildren(...result.items.map((item) => renderRecommendation(item, student, viewVersion)));
  } catch (error) {
    if (viewVersion === studentViewVersion) recommendationMessage.textContent = error.message;
  }
});

api("/mode").then((result) => {
  demo = result.demo;
  document.querySelector("#demo-views").hidden = !demo;
  document.querySelector("#demo-student-field").hidden = !demo;
  mode.textContent = demo ? "本地演示 · 独立样例数据" : "正式模式 · 请登录";
  showView(demo ? "manager" : "");
}).catch((error) => { message.textContent = error.message; });
