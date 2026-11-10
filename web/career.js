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

async function api(path, method = "GET", data) {
  const response = await fetch(prefix + path, {
    method,
    headers: { "X-Career-Demo": "1", "Content-Type": "application/json" },
    body: data === undefined ? undefined : JSON.stringify(data),
  });
  const body = await response.json();
  if (!response.ok) {
    const error = new Error(body.error);
    error.status = response.status;
    throw error;
  }
  return body;
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

editJob();
loadJobs().then(() => { jobFields.disabled = false; }).catch((error) => {
  jobsMessage.textContent = error.message;
});
loadStrategy();
