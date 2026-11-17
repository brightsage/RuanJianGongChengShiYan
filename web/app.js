/**
 * 高校学生成长与职业发展数据中台 - 商业级全域业务数据层 (Enterprise Business Data Layer)
 * 100% 严密对齐 C++ 后端模型 (D:\Develop\RuanJianGongChengShiYan\src\modules\academic_alerts\alerts_store.cpp)
 * 支持与后端无缝双向映射，并在离线脱机时提供完整高保真数据沙箱
 */

// ============================================================================
// 1. 全校纳管学生主数据 (完全对齐 C++ 后端 12 名真实在册学生种子数据)
// ============================================================================
const STUDENTS_MASTER = [
  {
    id: "2021001",
    name: "张伟",
    gender: "男",
    class: "计科2101",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "130102200301150012",
    phone: "13800138001",
    avgScore: 58.4,
    failedCourses: 3,
    creditRate: 0.52,
    attendance: 0.72,
    advisor: "李老师",
    radar: { academic: 58, project: 78, research: 45, comprehensive: 62, discipline: 65, teamwork: 70 },
    trends: [
      { term: "2024-2025-1", score: 61.2 },
      { term: "2024-2025-2", score: 57.8 },
      { term: "2025-2026-1", score: 58.4 }
    ],
    labels: [
      { text: "学业橙色预警", type: "danger", metric: "不及格 3 门 · 均分 58.4", formula: "挂科数 >= 2 && 绩点 < 60", source: "教务系统期末成绩库", date: "2025-11-10" },
      { text: "工程实践敏锐", type: "primary", metric: "项目实践分 78", formula: "软件工程实践课大作业优秀", source: "实验教学中心", date: "2025-10-18" },
      { text: "出勤偏低待帮扶", type: "warning", metric: "考勤打卡率 72%", formula: "课堂蓝牙打卡与门禁汇总", source: "智慧校园学工系统", date: "2025-11-05" }
    ],
    courses: [
      { code: "CS101", name: "高级语言程序设计 (C++)", credits: 4, score: 52, status: "不及格 (待重修)" },
      { code: "CS201", name: "数据结构与算法", credits: 4, score: 56, status: "不及格 (待补考)" },
      { code: "CS301", name: "计算机组成原理", credits: 3.5, score: 54, status: "不及格 (待补考)" },
      { code: "CS402", name: "软件工程综合实践", credits: 3, score: 82, status: "已通过" },
      { code: "CS505", name: "操作系统原理", credits: 3.5, score: 68, status: "已通过" }
    ]
  },
  {
    id: "2021002",
    name: "李娜",
    gender: "女",
    class: "计科2101",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "110105200305210045",
    phone: "13900139002",
    avgScore: 76.9,
    failedCourses: 1,
    creditRate: 0.86,
    attendance: 0.93,
    advisor: "李老师",
    radar: { academic: 77, project: 82, research: 70, comprehensive: 80, discipline: 92, teamwork: 85 },
    trends: [
      { term: "2024-2025-1", score: 74.1 },
      { term: "2024-2025-2", score: 75.6 },
      { term: "2025-2026-1", score: 76.9 }
    ],
    labels: [
      { text: "稳步发展型", type: "success", metric: "学分完成率 86%", formula: "修满专业学分比例", source: "教务处培养方案库", date: "2025-11-01" },
      { text: "全勤模范生", type: "success", metric: "出勤率 93%", formula: "智慧考勤系统无旷课", source: "学工实践系统", date: "2025-11-12" }
    ],
    courses: [
      { code: "CS101", name: "高级语言程序设计 (C++)", credits: 4, score: 78, status: "已通过" },
      { code: "CS201", name: "数据结构与算法", credits: 4, score: 81, status: "已通过" },
      { code: "CS301", name: "计算机组成原理", credits: 3.5, score: 58, status: "及格边缘" },
      { code: "CS402", name: "Web前端工程开发", credits: 3, score: 88, status: "优秀" }
    ]
  },
  {
    id: "2021003",
    name: "王强",
    gender: "男",
    class: "计科2101",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "320106200211080031",
    phone: "13700137003",
    avgScore: 52.1,
    failedCourses: 4,
    creditRate: 0.41,
    attendance: 0.65,
    advisor: "李老师",
    radar: { academic: 50, project: 58, research: 40, comprehensive: 55, discipline: 60, teamwork: 58 },
    trends: [
      { term: "2024-2025-1", score: 55.6 },
      { term: "2024-2025-2", score: 50.3 },
      { term: "2025-2026-1", score: 52.1 }
    ],
    labels: [
      { text: "红色极高危预警", type: "danger", metric: "挂科 4 门 · 出勤 65%", formula: "不及格 >= 3 || 出勤率 < 70%", source: "教务学工联合研判", date: "2025-11-12" },
      { text: "重点帮扶对象", type: "danger", metric: "学分完成率仅 41%", formula: "低于年级培养基线 30%", source: "学籍管理中心", date: "2025-11-01" }
    ],
    courses: [
      { code: "CS101", name: "高级语言程序设计 (C++)", credits: 4, score: 48, status: "不及格" },
      { code: "CS201", name: "数据结构与算法", credits: 4, score: 51, status: "不及格" },
      { code: "CS202", name: "离散数学", credits: 3, score: 45, status: "不及格" },
      { code: "CS301", name: "计算机体系结构", credits: 3, score: 52, status: "不及格" }
    ]
  },
  {
    id: "2021004",
    name: "赵敏",
    gender: "女",
    class: "计科2101",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "310104200308190022",
    phone: "13600136004",
    avgScore: 68.7,
    failedCourses: 0,
    creditRate: 0.74,
    attendance: 0.88,
    advisor: "李老师",
    radar: { academic: 70, project: 75, research: 65, comprehensive: 72, discipline: 88, teamwork: 80 },
    trends: [
      { term: "2024-2025-1", score: 70.2 },
      { term: "2024-2025-2", score: 69.5 },
      { term: "2025-2026-1", score: 68.7 }
    ],
    labels: [
      { text: "全科达标", type: "success", metric: "挂科 0 门", formula: "期末考试零挂科", source: "教务系统", date: "2025-11-01" },
      { text: "良好学术品格", type: "primary", metric: "考勤 88%", formula: "课堂考勤合规", source: "学工系统", date: "2025-11-08" }
    ],
    courses: [
      { code: "CS101", name: "程序设计基石", credits: 4, score: 72, status: "已通过" },
      { code: "CS201", name: "数据结构", credits: 4, score: 68, status: "已通过" }
    ]
  },
  {
    id: "2021005",
    name: "陈晨",
    gender: "男",
    class: "计科2102",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "440106200212030058",
    phone: "13500135005",
    avgScore: 63.5,
    failedCourses: 2,
    creditRate: 0.68,
    attendance: 0.79,
    advisor: "张老师",
    radar: { academic: 64, project: 70, research: 55, comprehensive: 68, discipline: 74, teamwork: 72 },
    trends: [
      { term: "2024-2025-1", score: 66.8 },
      { term: "2024-2025-2", score: 64.1 },
      { term: "2025-2026-1", score: 63.5 }
    ],
    labels: [
      { text: "学业橙色预警 (已超期)", type: "danger", metric: "不及格 2 门 · 超期 72h", formula: "挂科 >= 2 且触发超过 48h 未结案", source: "学业风险规则引擎", date: "2025-11-08" }
    ],
    courses: [
      { code: "CS201", name: "数据结构与算法", credits: 4, score: 55, status: "不及格" },
      { code: "CS301", name: "计算机组成原理", credits: 3.5, score: 58, status: "不及格" }
    ]
  },
  {
    id: "2021006",
    name: "刘洋",
    gender: "男",
    class: "计科2102",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "420102200304140071",
    phone: "13400134006",
    avgScore: 81.3,
    failedCourses: 0,
    creditRate: 0.92,
    attendance: 0.96,
    advisor: "张老师",
    radar: { academic: 85, project: 88, research: 78, comprehensive: 84, discipline: 95, teamwork: 86 },
    trends: [
      { term: "2024-2025-1", score: 79.4 },
      { term: "2024-2025-2", score: 80.6 },
      { term: "2025-2026-1", score: 81.3 }
    ],
    labels: [
      { text: "国家奖学金潜质", type: "success", metric: "加权绩点 81.3 · 零挂科", formula: "绩点 > 80 && 挂科 = 0", source: "评奖评优档案", date: "2025-11-01" },
      { text: "开源实践活跃", type: "primary", metric: "代码提交量年级前 5%", formula: "Git 仓库协同分析", source: "实践平台", date: "2025-10-25" }
    ],
    courses: [
      { code: "CS101", name: "C++ 高级编程", credits: 4, score: 89, status: "优秀" },
      { code: "CS201", name: "算法分析与设计", credits: 4, score: 84, status: "优秀" }
    ]
  },
  {
    id: "2021007",
    name: "周杰",
    gender: "男",
    class: "计科2102",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "510107200209280039",
    phone: "13300133007",
    avgScore: 47.8,
    failedCourses: 5,
    creditRate: 0.33,
    attendance: 0.58,
    advisor: "张老师",
    radar: { academic: 42, project: 50, research: 35, comprehensive: 48, discipline: 52, teamwork: 50 },
    trends: [
      { term: "2024-2025-1", score: 52.3 },
      { term: "2024-2025-2", score: 49.1 },
      { term: "2025-2026-1", score: 47.8 }
    ],
    labels: [
      { text: "退学警示级风险", type: "danger", metric: "不及格 5 门 · 绩点 47.8", formula: "挂科 >= 4 || 绩点 < 50", source: "教务风险红头警告", date: "2025-11-14" },
      { text: "严重缺勤", type: "danger", metric: "到课率 58%", formula: "低于 60% 警戒底线", source: "学工打卡", date: "2025-11-10" }
    ],
    courses: [
      { code: "CS101", name: "高级语言程序设计", credits: 4, score: 42, status: "不及格" },
      { code: "CS201", name: "数据结构", credits: 4, score: 45, status: "不及格" },
      { code: "CS301", name: "操作系统", credits: 3.5, score: 50, status: "不及格" },
      { code: "CS401", name: "计算机网络", credits: 3.5, score: 49, status: "不及格" },
      { code: "CS202", name: "离散数学", credits: 3, score: 48, status: "不及格" }
    ]
  },
  {
    id: "2021008",
    name: "吴静",
    gender: "女",
    class: "计科2102",
    major: "计算机科学与技术",
    grade: "2021",
    idCard: "330106200303100063",
    phone: "13200132008",
    avgScore: 72.6,
    failedCourses: 1,
    creditRate: 0.81,
    attendance: 0.91,
    advisor: "张老师",
    radar: { academic: 74, project: 78, research: 62, comprehensive: 75, discipline: 89, teamwork: 82 },
    trends: [
      { term: "2024-2025-1", score: 71.9 },
      { term: "2024-2025-2", score: 72.0 },
      { term: "2025-2026-1", score: 72.6 }
    ],
    labels: [
      { text: "学业关注", type: "warning", metric: "单科不及格 1 门", formula: "挂科 = 1", source: "教务系统", date: "2025-11-01" },
      { text: "纪律优良", type: "success", metric: "出勤 91%", formula: "无旷课违纪", source: "学工系统", date: "2025-11-05" }
    ],
    courses: [
      { code: "CS101", name: "C++ 程序设计", credits: 4, score: 76, status: "已通过" },
      { code: "CS301", name: "编译原理", credits: 3, score: 57, status: "不及格 (待补考)" }
    ]
  },
  {
    id: "2021009",
    name: "孙悦",
    gender: "女",
    class: "软工2101",
    major: "软件工程",
    grade: "2021",
    idCard: "210104200307180026",
    phone: "13100131009",
    avgScore: 66.2,
    failedCourses: 1,
    creditRate: 0.71,
    attendance: 0.77,
    advisor: "王老师",
    radar: { academic: 68, project: 72, research: 58, comprehensive: 70, discipline: 78, teamwork: 76 },
    trends: [
      { term: "2024-2025-1", score: 68.0 },
      { term: "2024-2025-2", score: 67.1 },
      { term: "2025-2026-1", score: 66.2 }
    ],
    labels: [
      { text: "常规黄色预警", type: "warning", metric: "考勤 77% (低于80%)", formula: "出勤率 < 80%", source: "智慧教室物联网", date: "2025-11-10" }
    ],
    courses: [
      { code: "SE101", name: "软件需求工程", credits: 3, score: 72, status: "已通过" },
      { code: "SE201", name: "软件体系结构", credits: 3.5, score: 55, status: "不及格" }
    ]
  },
  {
    id: "2021010",
    name: "郑凯",
    gender: "男",
    class: "软工2101",
    major: "软件工程",
    grade: "2021",
    idCard: "610103200210090017",
    phone: "13000130010",
    avgScore: 55.3,
    failedCourses: 3,
    creditRate: 0.48,
    attendance: 0.69,
    advisor: "王老师",
    radar: { academic: 54, project: 65, research: 45, comprehensive: 58, discipline: 66, teamwork: 62 },
    trends: [
      { term: "2024-2025-1", score: 58.9 },
      { term: "2024-2025-2", score: 56.4 },
      { term: "2025-2026-1", score: 55.3 }
    ],
    labels: [
      { text: "学业橙色预警", type: "danger", metric: "挂科 3 门 · 绩点 55.3", formula: "挂科 >= 2", source: "教务处", date: "2025-11-12" }
    ],
    courses: [
      { code: "SE101", name: "软件设计模式", credits: 3, score: 51, status: "不及格" },
      { code: "SE201", name: "数据库原理与应用", credits: 4, score: 54, status: "不及格" }
    ]
  },
  {
    id: "2021011",
    name: "冯雪",
    gender: "女",
    class: "软工2101",
    major: "软件工程",
    grade: "2021",
    idCard: "120101200306120048",
    phone: "18900189011",
    avgScore: 88.1,
    failedCourses: 0,
    creditRate: 0.95,
    attendance: 0.98,
    advisor: "王老师",
    radar: { academic: 92, project: 94, research: 86, comprehensive: 90, discipline: 98, teamwork: 92 },
    trends: [
      { term: "2024-2025-1", score: 86.2 },
      { term: "2024-2025-2", score: 87.4 },
      { term: "2025-2026-1", score: 88.1 }
    ],
    labels: [
      { text: "学院拔尖卓越人才", type: "success", metric: "均分 88.1 · 挂科 0 门", formula: "年级前 1%", source: "教务特奖库", date: "2025-11-01" },
      { text: "全栈架构大师", type: "primary", metric: "工程能力得分 94", formula: "竞赛一等奖", source: "学科竞赛中心", date: "2025-10-15" }
    ],
    courses: [
      { code: "SE101", name: "现代软件工程", credits: 4, score: 95, status: "优秀" },
      { code: "SE201", name: "分布式系统架构", credits: 4, score: 92, status: "优秀" },
      { code: "SE301", name: "云计算与微服务", credits: 3.5, score: 90, status: "优秀" }
    ]
  },
  {
    id: "2021012",
    name: "何磊",
    gender: "男",
    class: "软工2101",
    major: "软件工程",
    grade: "2021",
    idCard: "370102200208240035",
    phone: "18800188012",
    avgScore: 61.9,
    failedCourses: 2,
    creditRate: 0.63,
    attendance: 0.75,
    advisor: "王老师",
    radar: { academic: 62, project: 74, research: 52, comprehensive: 66, discipline: 72, teamwork: 70 },
    trends: [
      { term: "2024-2025-1", score: 64.0 },
      { term: "2024-2025-2", score: 62.7 },
      { term: "2025-2026-1", score: 61.9 }
    ],
    labels: [
      { text: "学业关注预警", type: "warning", metric: "挂科 2 门 · 考勤 75%", formula: "挂科 >= 2 || 考勤 < 80%", source: "学业风险规则引擎", date: "2025-11-13" }
    ],
    courses: [
      { code: "SE101", name: "Java 企业级开发", credits: 4, score: 74, status: "已通过" },
      { code: "SE201", name: "操作系统实现", credits: 3.5, score: 56, status: "不及格" }
    ]
  }
];

// ============================================================================
// 2. 预警规则集合 (完全对齐 C++ 后端 seed_rules)
// ============================================================================
const INITIAL_RULES = [
  { id: "R1", name: "综合成绩不达标", metric: "avg_score", operator: "lt", threshold: 60.0, level: "red", notify: "辅导员、学生本人", enabled: true },
  { id: "R2", name: "多门课程不及格", metric: "failed_courses", operator: "gte", threshold: 2.0, level: "orange", notify: "辅导员", enabled: true },
  { id: "R3", name: "出勤率偏低", metric: "attendance", operator: "lt", threshold: 0.80, level: "yellow", notify: "辅导员", enabled: true },
  { id: "R4", name: "学分完成率偏低", metric: "credit_rate", operator: "lt", threshold: 0.60, level: "orange", notify: "辅导员、院系管理员", enabled: true }
];

// ============================================================================
// 3. 初始学业预警记录 (完全对齐 C++ 后端 seed_alerts: 陈晨 72h 前生成，触发逾期告警)
// ============================================================================
const INITIAL_ALERTS = [
  {
    id: 1,
    studentId: "2021005",
    studentName: "陈晨",
    class: "计科2102",
    major: "计算机科学与技术",
    ruleId: "R2",
    ruleName: "多门课程不及格",
    level: "orange",
    status: "open",
    metricValue: "挂科 2 门",
    threshold: 2.0,
    created: Date.now() - 72 * 3600 * 1000, // 3 天前生成，触发 >48h 超时督导
    assignee: null,
    closedAt: null,
    interventions: []
  }
];

// ============================================================================
// 4. 行业权威岗位胜任力画像库 (人岗匹配智能推荐算法基准)
// ============================================================================
const JOBS_DATA = [
  {
    id: "job-01",
    name: "现代 C++ / 基础架构研发工程师",
    industry: "基础软件与大模型基础设施",
    degree: "本科及以上",
    salary: "20K - 35K · 16薪",
    desc: "负责高性能分布式存储、编译构建系统与内嵌式高吞吐 HTTP 服务研发，需精通现代 C++ 标准与网络编程。",
    abilities: [
      { name: "现代 C++ / 系统编程", reqScore: 75, weight: 0.35 },
      { name: "计算机网络与并发", reqScore: 70, weight: 0.25 },
      { name: "算法与复杂度分析", reqScore: 70, weight: 0.25 },
      { name: "团队敏捷工程协同", reqScore: 65, weight: 0.15 }
    ]
  },
  {
    id: "job-02",
    name: "全栈云原生平台架构师",
    industry: "云计算与企业级 SaaS",
    degree: "本科及以上",
    salary: "25K - 40K · 15薪",
    desc: "负责高校数字化中台与企业 SaaS 云服务全流程研发，精通高并发 RESTful API 设计、数据治理与前端可视化交互。",
    abilities: [
      { name: "Web 全栈架构与微服务", reqScore: 80, weight: 0.35 },
      { name: "数据湖仓与主数据治理", reqScore: 70, weight: 0.25 },
      { name: "工程实践交付素养", reqScore: 75, weight: 0.25 },
      { name: "跨部门业务沟通", reqScore: 70, weight: 0.15 }
    ]
  },
  {
    id: "job-03",
    name: "算法工程与智能决策研究员",
    industry: "人工智能与学情大数据",
    degree: "硕士 / 优秀本科",
    salary: "28K - 50K · 16薪",
    desc: "主导群体学业风险智能归因、因果推断图谱与精准就业匹配模型构建，具备卓越数学与创新素养。",
    abilities: [
      { name: "学术科研与数学建模", reqScore: 80, weight: 0.40 },
      { name: "复杂算法优化实践", reqScore: 75, weight: 0.30 },
      { name: "行业画像特征挖掘", reqScore: 70, weight: 0.20 },
      { name: "国际前沿追踪视野", reqScore: 65, weight: 0.10 }
    ]
  },
  {
    id: "job-04",
    name: "大数据平台开发工程师",
    industry: "数据中台与工业互联网",
    degree: "本科及以上",
    salary: "18K - 30K · 14薪",
    desc: "负责一人一档主数据湖建设、增量 CDC 接入管道调度与元数据质量管控，消除一数多源数据孤岛。",
    abilities: [
      { name: "分布式计算与数据管道", reqScore: 75, weight: 0.35 },
      { name: "数据库底层与 SQL 调优", reqScore: 75, weight: 0.30 },
      { name: "数据血缘与质量治理", reqScore: 70, weight: 0.20 },
      { name: "系统安全合规意识", reqScore: 70, weight: 0.15 }
    ]
  }
];

// ============================================================================
// 5. 数据服务开放网关目录 (Open API Catalog)
// ============================================================================
const DATA_SERVICES_CATALOG = [
  {
    id: "svc-alerts-dash",
    name: "学业预警全校宏观态势看板",
    method: "GET",
    path: "/api/modules/academic-alerts/dashboard",
    version: "v2.5",
    desc: "聚合全校学生加权均分、不及格分布、待处理与超期告警总数，支持按班级/专业/年级动态下钻。",
    scope: "scope:dashboard:read",
    qpsLimit: 500
  },
  {
    id: "svc-alerts-list",
    name: "在管学业风险预警记录检索",
    method: "GET",
    path: "/api/modules/academic-alerts/alerts",
    version: "v2.5",
    desc: "多维检索红橙黄分级预警台账，标记超 48 小时逾期未处置状态，返回学生关联全景信息。",
    scope: "scope:alerts:manage",
    qpsLimit: 300
  },
  {
    id: "svc-alerts-handle",
    name: "学业风险处置与帮扶措施留痕",
    method: "POST",
    path: "/api/modules/academic-alerts/alerts/:alert_id/handle",
    version: "v2.5",
    desc: "支持结案归档 (close)、面对面谈话跟进 (follow) 及标记不适用 (not_applicable)，强制防篡改审计留痕。",
    scope: "scope:interventions:write",
    qpsLimit: 100
  },
  {
    id: "svc-student-trend",
    name: "学生历年绩点趋势回溯",
    method: "GET",
    path: "/api/modules/academic-alerts/students/:student_id/trend",
    version: "v2.5",
    desc: "拉取指定学生多学期学业绩点历史演进曲线，供全息画像平滑演进折线图渲染。",
    scope: "scope:student:profile",
    qpsLimit: 200
  }
];


/**
 * 高校学生成长与职业发展数据中台 - 商业级智能 API 服务层 (Platform API Service)
 * 具备「真实 C++ 后端直连 + 离线全真模拟沙箱」双模无缝自适应架构
 * 
 * 当运行在 C++ 服务端 (127.0.0.1:8080) 时，自动直连真实 RESTful 路由；
 * 当离线、独立双击打开或后端未启动时，自动无缝降级为高保真内存/持久化沙箱，绝无控制台报错。
 */

class PlatformApiClient {
  constructor() {
    this.baseUrl = window.location.origin.startsWith("http") ? "" : "http://127.0.0.1:8080";
    this.isOnline = false;
    this.lastLatency = 0;
    this.onStatusChange = null;
  }

  /**
   * 探测 C++ 服务端健康度
   */
  async checkConnection() {
    const t0 = performance.now();
    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 1200);
      const res = await fetch(`${this.baseUrl}/api/health`, {
        method: "GET",
        signal: controller.signal
      });
      clearTimeout(timeoutId);
      if (res.ok) {
        const data = await res.json();
        if (data.status === "ok") {
          this.isOnline = true;
          this.lastLatency = Math.max(1, Math.round(performance.now() - t0));
          if (this.onStatusChange) this.onStatusChange(true, this.lastLatency);
          return true;
        }
      }
    } catch (e) {
      // 离线环境正常处理，不抛出异常
    }
    this.isOnline = false;
    this.lastLatency = 0;
    if (this.onStatusChange) this.onStatusChange(false, 0);
    return false;
  }

  // ---------------------------------------------------------------------------
  // M4 学业分析与预警 API 直连
  // ---------------------------------------------------------------------------

  /**
   * 获取宏观态势与群体下钻看板
   * GET /api/modules/academic-alerts/dashboard?group=class|major|grade&class=...&major=...
   */
  async getDashboard(group = "class", filterClass = "", filterMajor = "") {
    if (this.isOnline) {
      try {
        const params = new URLSearchParams();
        if (group) params.append("group", group);
        if (filterClass && filterClass !== "all") params.append("class", filterClass);
        if (filterMajor && filterMajor !== "all") params.append("major", filterMajor);
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/dashboard?${params.toString()}`);
        if (res.ok) return await res.json();
      } catch (e) {
        console.warn("直连后端 dashboard 降级为沙箱", e);
      }
    }
    return null; // 触发本地沙箱兜底
  }

  /**
   * 获取规则清单
   * GET /api/modules/academic-alerts/rules
   */
  async getRules() {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/rules`);
        if (res.ok) {
          const data = await res.json();
          return data.rules || [];
        }
      } catch (e) {
        console.warn("直连后端 rules 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 创建预警规则
   * POST /api/modules/academic-alerts/rules
   */
  async createRule(ruleObj) {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/rules`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(ruleObj)
        });
        if (res.ok) return await res.json();
      } catch (e) {
        console.warn("直连后端 createRule 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 全量扫描与生成预警
   * POST /api/modules/academic-alerts/alerts/generate
   */
  async generateAlerts() {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/alerts/generate`, {
          method: "POST"
        });
        if (res.ok) return await res.json();
      } catch (e) {
        console.warn("直连后端 generateAlerts 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 查询预警清单
   * GET /api/modules/academic-alerts/alerts
   */
  async getAlerts(filters = {}) {
    if (this.isOnline) {
      try {
        const params = new URLSearchParams();
        if (filters.status && filters.status !== "all") params.append("status", filters.status);
        if (filters.level && filters.level !== "all") params.append("level", filters.level);
        if (filters.class && filters.class !== "all") params.append("class", filters.class);
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/alerts?${params.toString()}`);
        if (res.ok) {
          const data = await res.json();
          return data.alerts || [];
        }
      } catch (e) {
        console.warn("直连后端 getAlerts 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 认领预警
   * POST /api/modules/academic-alerts/alerts/:alert_id/claim
   */
  async claimAlert(alertId) {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/alerts/${alertId}/claim`, {
          method: "POST"
        });
        if (res.ok) return await res.json();
      } catch (e) {
        console.warn("直连后端 claimAlert 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 处置预警与干预留痕
   * POST /api/modules/academic-alerts/alerts/:alert_id/handle
   */
  async handleAlert(alertId, payload) {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/alerts/${alertId}/handle`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload)
        });
        if (res.ok) return await res.json();
      } catch (e) {
        console.warn("直连后端 handleAlert 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 查询干预历史
   * GET /api/modules/academic-alerts/alerts/:alert_id/interventions
   */
  async getInterventions(alertId) {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/alerts/${alertId}/interventions`);
        if (res.ok) {
          const data = await res.json();
          return data.interventions || [];
        }
      } catch (e) {
        console.warn("直连后端 getInterventions 降级为沙箱", e);
      }
    }
    return null;
  }

  /**
   * 查询学生绩点趋势
   * GET /api/modules/academic-alerts/students/:student_id/trend
   */
  async getStudentTrend(studentId) {
    if (this.isOnline) {
      try {
        const res = await fetch(`${this.baseUrl}/api/modules/academic-alerts/students/${studentId}/trend`);
        if (res.ok) {
          const data = await res.json();
          return data.trend || [];
        }
      } catch (e) {
        console.warn("直连后端 getStudentTrend 降级为沙箱", e);
      }
    }
    return null;
  }
}

window.PlatformApiClient = PlatformApiClient;


/**
 * 高校学生成长与职业发展数据中台 - 60FPS 流畅动效 Retina Canvas 图表渲染引擎 (Pure Light Luxury Edition)
 * 具备平滑缓动入场动画 (Easing In/Out)、明亮清爽微晶渲染、贝塞尔流线与高对比度排版
 */

class ModernChartEngine {
  static easeOutCubic(t) {
    return 1 - Math.pow(1 - t, 3);
  }

  static setupCanvas(canvas) {
    if (!canvas) return null;
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const width = rect.width || canvas.width || 360;
    const height = rect.height || canvas.height || 260;
    
    canvas.width = width * dpr;
    canvas.height = height * dpr;
    
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    return { ctx, width, height, dpr };
  }

  /**
   * 1. 动效六维能力素养雷达图 (Animated Radar Chart - Light Mode)
   */
  static renderRadar(canvasId, config) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    
    if (canvas._radarAnimId) cancelAnimationFrame(canvas._radarAnimId);

    const labels = config.labels || ['学业素养', '工程实践', '科研创新', '通识素质', '行为纪律', '团队协作'];
    const totalSides = labels.length;
    const levels = 5;
    const values = config.values || [70, 70, 70, 70, 70, 70];
    const bVals = config.benchmark ? config.benchmark.values : [72, 65, 58, 68, 82, 75];

    let startTime = null;
    const duration = 650; // 650ms 缓动展开

    const animate = (timestamp) => {
      if (!startTime) startTime = timestamp;
      const elapsed = timestamp - startTime;
      const progress = Math.min(1, elapsed / duration);
      const easeProgress = this.easeOutCubic(progress);

      const setup = this.setupCanvas(canvas);
      if (!setup) return;
      const { ctx, width, height } = setup;
      ctx.clearRect(0, 0, width, height);

      const centerX = width / 2;
      const centerY = height / 2;
      const radius = Math.min(centerX, centerY) - 44;

      // 1. 同心多边形网格 (Light Mode)
      for (let l = 1; l <= levels; l++) {
        const curR = (radius / levels) * l;
        ctx.beginPath();
        for (let i = 0; i < totalSides; i++) {
          const angle = (Math.PI * 2 / totalSides) * i - Math.PI / 2;
          const x = centerX + curR * Math.cos(angle);
          const y = centerY + curR * Math.sin(angle);
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.closePath();
        ctx.strokeStyle = l === levels ? 'rgba(0, 0, 0, 0.12)' : 'rgba(0, 0, 0, 0.05)';
        ctx.lineWidth = l === levels ? 1.5 : 1;
        ctx.stroke();

        if (l % 2 === 1) {
          ctx.fillStyle = 'rgba(0, 0, 0, 0.015)';
          ctx.fill();
        }

        if (l === 3 || l === 5) {
          ctx.fillStyle = '#64748b';
          ctx.font = '10px var(--font-mono, monospace)';
          ctx.textAlign = 'left';
          ctx.fillText(`${(100 / levels) * l}`, centerX + 4, centerY - curR + 10);
        }
      }

      // 2. 轴线与文字标签
      for (let i = 0; i < totalSides; i++) {
        const angle = (Math.PI * 2 / totalSides) * i - Math.PI / 2;
        const x = centerX + radius * Math.cos(angle);
        const y = centerY + radius * Math.sin(angle);

        ctx.beginPath();
        ctx.moveTo(centerX, centerY);
        ctx.lineTo(x, y);
        ctx.strokeStyle = 'rgba(0, 0, 0, 0.06)';
        ctx.lineWidth = 1;
        ctx.stroke();

        const labelX = centerX + (radius + 24) * Math.cos(angle);
        const labelY = centerY + (radius + 20) * Math.sin(angle);

        ctx.fillStyle = '#1e293b';
        ctx.font = '600 12px -apple-system, sans-serif';
        ctx.textAlign = Math.abs(Math.cos(angle)) < 0.25 ? 'center' : (Math.cos(angle) > 0 ? 'left' : 'right');
        ctx.textBaseline = Math.abs(Math.sin(angle)) < 0.25 ? 'middle' : (Math.sin(angle) > 0 ? 'top' : 'bottom');
        ctx.fillText(labels[i], labelX, labelY);
      }

      // 3. 标杆对比层 (年级专业均值 - 优雅虚线)
      if (bVals) {
        ctx.beginPath();
        for (let i = 0; i < totalSides; i++) {
          const val = Math.min(100, Math.max(0, bVals[i] || 0));
          const r = (radius * val * easeProgress) / 100;
          const angle = (Math.PI * 2 / totalSides) * i - Math.PI / 2;
          const x = centerX + r * Math.cos(angle);
          const y = centerY + r * Math.sin(angle);
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.closePath();
        ctx.strokeStyle = 'rgba(100, 116, 139, 0.6)';
        ctx.setLineDash([4, 4]);
        ctx.lineWidth = 1.5;
        ctx.stroke();
        ctx.setLineDash([]);
      }

      // 4. 当前学生实际多边形 (随 easeProgress 从中心扩散)
      ctx.beginPath();
      const points = [];
      for (let i = 0; i < totalSides; i++) {
        const val = Math.min(100, Math.max(0, values[i] || 0));
        const r = (radius * val * easeProgress) / 100;
        const angle = (Math.PI * 2 / totalSides) * i - Math.PI / 2;
        const x = centerX + r * Math.cos(angle);
        const y = centerY + r * Math.sin(angle);
        points.push({ x, y, val });
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.closePath();

      // 柔和亮色紫罗兰填充
      const grad = ctx.createRadialGradient(centerX, centerY, 5, centerX, centerY, radius);
      grad.addColorStop(0, 'rgba(79, 70, 229, 0.25)');
      grad.addColorStop(1, 'rgba(124, 58, 237, 0.08)');
      ctx.fillStyle = grad;
      ctx.fill();

      // 精致描边
      ctx.save();
      ctx.strokeStyle = '#4f46e5';
      ctx.lineWidth = 2.5;
      ctx.stroke();
      ctx.restore();

      // 节点微晶点
      points.forEach(pt => {
        ctx.beginPath();
        ctx.arc(pt.x, pt.y, 4.5, 0, Math.PI * 2);
        ctx.fillStyle = '#ffffff';
        ctx.fill();
        ctx.lineWidth = 2.5;
        ctx.strokeStyle = '#4f46e5';
        ctx.stroke();
      });

      if (progress < 1) {
        canvas._radarAnimId = requestAnimationFrame(animate);
      }
    };

    canvas._radarAnimId = requestAnimationFrame(animate);
  }

  /**
   * 2. 动效升起柱状图 (Animated Bar Chart - Light Mode)
   */
  static renderBar(canvasId, config) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    if (canvas._barAnimId) cancelAnimationFrame(canvas._barAnimId);

    const data = config.data || [];
    if (data.length === 0) return;
    const maxVal = config.maxVal || 100;

    let startTime = null;
    const duration = 600;

    const animate = (timestamp) => {
      if (!startTime) startTime = timestamp;
      const elapsed = timestamp - startTime;
      const progress = Math.min(1, elapsed / duration);
      const easeProgress = this.easeOutCubic(progress);

      const setup = this.setupCanvas(canvas);
      if (!setup) return;
      const { ctx, width, height } = setup;
      ctx.clearRect(0, 0, width, height);

      const padLeft = 45;
      const padRight = 20;
      const padTop = 30;
      const padBottom = 40;
      const plotW = width - padLeft - padRight;
      const plotH = height - padTop - padBottom;
      const yTicks = 4;

      // Y 轴刻度与辅助线
      ctx.strokeStyle = 'rgba(0, 0, 0, 0.05)';
      ctx.fillStyle = '#64748b';
      ctx.font = '10px var(--font-mono, monospace)';
      ctx.textAlign = 'right';

      for (let i = 0; i <= yTicks; i++) {
        const val = Math.round((maxVal / yTicks) * i);
        const y = padTop + plotH - (plotH / yTicks) * i;
        ctx.beginPath();
        ctx.moveTo(padLeft, y);
        ctx.lineTo(width - padRight, y);
        ctx.stroke();
        ctx.fillText(val, padLeft - 8, y + 3);
      }

      const barWidth = Math.min(46, (plotW / data.length) * 0.48);
      const gap = plotW / data.length;

      data.forEach((item, idx) => {
        const x = padLeft + gap * idx + (gap - barWidth) / 2;
        const targetH = (item.value / maxVal) * plotH;
        const currentH = targetH * easeProgress;
        const y = padTop + plotH - currentH;

        ctx.save();
        const barGrad = ctx.createLinearGradient(0, y, 0, padTop + plotH);
        const color = item.color || '#4f46e5';
        barGrad.addColorStop(0, color);
        barGrad.addColorStop(1, 'rgba(79, 70, 229, 0.15)');

        ctx.fillStyle = barGrad;

        // 倒圆角柱
        ctx.beginPath();
        const r = 6;
        ctx.moveTo(x + r, y);
        ctx.lineTo(x + barWidth - r, y);
        ctx.quadraticCurveTo(x + barWidth, y, x + barWidth, y + r);
        ctx.lineTo(x + barWidth, padTop + plotH);
        ctx.lineTo(x, padTop + plotH);
        ctx.lineTo(x, y + r);
        ctx.quadraticCurveTo(x, y, x + r, y);
        ctx.closePath();
        ctx.fill();
        ctx.restore();

        // 标顶数值
        ctx.fillStyle = '#0f172a';
        ctx.font = '700 12px var(--font-sans, sans-serif)';
        ctx.textAlign = 'center';
        const displayVal = (item.value * easeProgress).toFixed(1);
        ctx.fillText(displayVal, x + barWidth / 2, y - 8);

        // X 轴标签
        ctx.fillStyle = '#475569';
        ctx.font = '600 11.5px var(--font-sans, sans-serif)';
        ctx.fillText(item.label, x + barWidth / 2, padTop + plotH + 20);
      });

      if (progress < 1) {
        canvas._barAnimId = requestAnimationFrame(animate);
      }
    };

    canvas._barAnimId = requestAnimationFrame(animate);
  }

  /**
   * 3. 动效平滑流线图 (Animated Spline Line Chart - Light Mode)
   */
  static renderLine(canvasId, config) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    if (canvas._lineAnimId) cancelAnimationFrame(canvas._lineAnimId);

    const data = config.data || [];
    if (data.length === 0) return;

    const minVal = config.minVal !== undefined ? config.minVal : 40;
    const maxVal = config.maxVal !== undefined ? config.maxVal : 100;
    const range = maxVal - minVal;

    let startTime = null;
    const duration = 700;

    const animate = (timestamp) => {
      if (!startTime) startTime = timestamp;
      const elapsed = timestamp - startTime;
      const progress = Math.min(1, elapsed / duration);
      const easeProgress = this.easeOutCubic(progress);

      const setup = this.setupCanvas(canvas);
      if (!setup) return;
      const { ctx, width, height } = setup;
      ctx.clearRect(0, 0, width, height);

      const padLeft = 45;
      const padRight = 30;
      const padTop = 32;
      const padBottom = 40;
      const plotW = width - padLeft - padRight;
      const plotH = height - padTop - padBottom;
      const ySteps = 4;

      // 辅助刻度
      ctx.strokeStyle = 'rgba(0, 0, 0, 0.05)';
      ctx.fillStyle = '#64748b';
      ctx.font = '10px var(--font-mono, monospace)';
      ctx.textAlign = 'right';

      for (let i = 0; i <= ySteps; i++) {
        const val = Math.round(minVal + (range / ySteps) * i);
        const y = padTop + plotH - (plotH / ySteps) * i;
        ctx.beginPath();
        ctx.moveTo(padLeft, y);
        ctx.lineTo(width - padRight, y);
        ctx.stroke();
        ctx.fillText(val, padLeft - 8, y + 3);
      }

      // 坐标点计算
      const pts = data.map((d, i) => {
        const x = padLeft + (plotW / (data.length - 1 || 1)) * i;
        const targetY = padTop + plotH - ((d.value - minVal) / (range || 1)) * plotH;
        const y = padTop + plotH - (padTop + plotH - targetY) * easeProgress;
        return { x, y, label: d.label, val: d.value };
      });

      // 贝塞尔渐变面积底色
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      for (let i = 0; i < pts.length - 1; i++) {
        const xc = (pts[i].x + pts[i + 1].x) / 2;
        const yc = (pts[i].y + pts[i + 1].y) / 2;
        ctx.quadraticCurveTo(pts[i].x, pts[i].y, xc, yc);
      }
      ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y);
      ctx.lineTo(pts[pts.length - 1].x, padTop + plotH);
      ctx.lineTo(pts[0].x, padTop + plotH);
      ctx.closePath();

      const areaGrad = ctx.createLinearGradient(0, padTop, 0, padTop + plotH);
      areaGrad.addColorStop(0, 'rgba(79, 70, 229, 0.16)');
      areaGrad.addColorStop(1, 'rgba(79, 70, 229, 0.00)');
      ctx.fillStyle = areaGrad;
      ctx.fill();

      // 曲线描边
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      for (let i = 0; i < pts.length - 1; i++) {
        const xc = (pts[i].x + pts[i + 1].x) / 2;
        const yc = (pts[i].y + pts[i + 1].y) / 2;
        ctx.quadraticCurveTo(pts[i].x, pts[i].y, xc, yc);
      }
      ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y);
      ctx.strokeStyle = '#4f46e5';
      ctx.lineWidth = 3;
      ctx.stroke();
      ctx.restore();

      // 点位
      pts.forEach(p => {
        ctx.beginPath();
        ctx.arc(p.x, p.y, 5, 0, Math.PI * 2);
        ctx.fillStyle = '#ffffff';
        ctx.fill();
        ctx.lineWidth = 2.5;
        ctx.strokeStyle = '#4f46e5';
        ctx.stroke();

        ctx.fillStyle = '#0f172a';
        ctx.font = '700 11.5px var(--font-sans, sans-serif)';
        ctx.textAlign = 'center';
        ctx.fillText(p.val.toFixed(1), p.x, p.y - 10);

        ctx.fillStyle = '#475569';
        ctx.font = '500 11.5px var(--font-sans, sans-serif)';
        ctx.fillText(p.label, p.x, padTop + plotH + 20);
      });

      if (progress < 1) {
        canvas._lineAnimId = requestAnimationFrame(animate);
      }
    };

    canvas._lineAnimId = requestAnimationFrame(animate);
  }

  /**
   * 4. 动效旋转环形图 (Animated Donut Chart - Light Mode)
   */
  static renderDonut(canvasId, config) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    if (canvas._donutAnimId) cancelAnimationFrame(canvas._donutAnimId);

    const data = config.data || [];
    const total = data.reduce((acc, d) => acc + d.value, 0);
    if (total === 0) return;

    let startTime = null;
    const duration = 650;

    const animate = (timestamp) => {
      if (!startTime) startTime = timestamp;
      const elapsed = timestamp - startTime;
      const progress = Math.min(1, elapsed / duration);
      const easeProgress = this.easeOutCubic(progress);

      const setup = this.setupCanvas(canvas);
      if (!setup) return;
      const { ctx, width, height } = setup;
      ctx.clearRect(0, 0, width, height);

      const centerX = width * 0.42;
      const centerY = height / 2;
      const outerR = Math.min(centerX, centerY) - 20;
      const innerR = outerR * 0.66;

      let startAngle = -Math.PI / 2;
      const gapAngle = 0.04;
      const totalSweep = Math.PI * 2 * easeProgress;

      data.forEach(item => {
        const sliceAngle = (item.value / total) * totalSweep;
        if (sliceAngle <= 0) return;

        ctx.save();
        ctx.beginPath();
        ctx.arc(centerX, centerY, outerR, startAngle + gapAngle / 2, startAngle + sliceAngle - gapAngle / 2);
        ctx.arc(centerX, centerY, innerR, startAngle + sliceAngle - gapAngle / 2, startAngle + gapAngle / 2, true);
        ctx.closePath();

        ctx.fillStyle = item.color || '#4f46e5';
        ctx.fill();
        ctx.restore();

        startAngle += sliceAngle;
      });

      // 中心文字 (Light Mode)
      ctx.fillStyle = '#0f172a';
      ctx.font = '800 24px var(--font-sans, sans-serif)';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      const countVal = Math.round(total * easeProgress);
      ctx.fillText(`${countVal}`, centerX, centerY - 6);

      ctx.fillStyle = '#64748b';
      ctx.font = '600 11px var(--font-sans, sans-serif)';
      ctx.fillText(config.centerText || '总计', centerX, centerY + 16);

      // 右侧图例
      const legendX = width * 0.72;
      const itemH = 26;
      const startY = centerY - (data.length * itemH) / 2 + 10;

      data.forEach((item, idx) => {
        const y = startY + idx * itemH;
        ctx.beginPath();
        ctx.arc(legendX, y, 5, 0, Math.PI * 2);
        ctx.fillStyle = item.color;
        ctx.fill();

        ctx.fillStyle = '#334155';
        ctx.font = '500 12px var(--font-sans, sans-serif)';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'middle';
        ctx.fillText(`${item.label} (${item.value})`, legendX + 12, y);
      });

      if (progress < 1) {
        canvas._donutAnimId = requestAnimationFrame(animate);
      }
    };

    canvas._donutAnimId = requestAnimationFrame(animate);
  }
}

window.ModernChartEngine = ModernChartEngine;


/**
 * 高校学生成长与职业发展数据中台 - 商业级业务中枢 (Commercial Platform Engine)
 * 具备 C++ 后端 RESTful 接口直连 + 离线高保真持久化仿真沙箱双核架构
 * 
 * 核心驱动：
 * - 驾驶舱多维群体聚合 (按班级/专业/年级动态下钻)
 * - 规则引擎配置器 (查看/动态新建预警规则)
 * - 预警状态机闭环 (全量研判/认领/超时督导/强制措施留痕/轨迹穿透)
 * - 学生 360° 全息档案 (12 名在册学员/雷达图/走势曲线/可解释性溯源/成长鉴定表打印)
 * - 岗位胜任力人岗匹配算法推荐
 * - 数据资产与主数据治理湖仓
 * - 数据开放网关与在线交互式 API 沙箱
 */

/**
 * 高性能 60FPS 数字滚动递增器 (EaseOutCubic)
 */
function animateCount(element, target, duration = 800, isFloat = false, suffix = "", prefix = "") {
  if (!element) return;
  const targetNum = Number(target) || 0;
  const startNum = 0;
  const startTime = performance.now();
  
  if (element._animId) {
    cancelAnimationFrame(element._animId);
  }

  function update(currentTime) {
    const elapsed = currentTime - startTime;
    const progress = Math.min(elapsed / duration, 1);
    const ease = 1 - Math.pow(1 - progress, 3);
    const current = startNum + (targetNum - startNum) * ease;
    
    if (isFloat) {
      element.textContent = `${prefix}${current.toFixed(1)}${suffix}`;
    } else {
      element.textContent = `${prefix}${Math.round(current).toLocaleString()}${suffix}`;
    }
    
    if (progress < 1) {
      element._animId = requestAnimationFrame(update);
    } else {
      if (isFloat) {
        element.textContent = `${prefix}${targetNum.toFixed(1)}${suffix}`;
      } else {
        element.textContent = `${prefix}${Math.round(targetNum).toLocaleString()}${suffix}`;
      }
      element._animId = null;
    }
  }
  
  element._animId = requestAnimationFrame(update);
}

class PlatformApp {
  constructor() {
    this.api = new PlatformApiClient();
    this.students = [...STUDENTS_MASTER];
    this.currentStudentId = localStorage.getItem("unigrowth_active_student") || "2021001";
    this.currentRole = localStorage.getItem("unigrowth_active_role") || "李老师（辅导员）";
    this.isDesensitized = localStorage.getItem("unigrowth_desensitize") !== "false";
    this.currentDashboardGroup = "class"; // 'class' | 'major' | 'grade'

    this.loadState();
    this.bindEvents();
    this.initApiConnection();
  }

  loadState() {
    const savedRules = localStorage.getItem("unigrowth_rules");
    this.rules = savedRules ? JSON.parse(savedRules) : JSON.parse(JSON.stringify(INITIAL_RULES));

    const savedAlerts = localStorage.getItem("unigrowth_alerts");
    this.alerts = savedAlerts ? JSON.parse(savedAlerts) : JSON.parse(JSON.stringify(INITIAL_ALERTS));
  }

  saveRules() {
    localStorage.setItem("unigrowth_rules", JSON.stringify(this.rules));
  }

  saveAlerts() {
    localStorage.setItem("unigrowth_alerts", JSON.stringify(this.alerts));
  }

  async initApiConnection() {
    this.api.onStatusChange = (isOnline, latency) => {
      this.updateConnectionBadge(isOnline, latency);
    };
    await this.api.checkConnection();
  }

  updateConnectionBadge(isOnline, latency) {
    const el = document.getElementById("top-connection-status");
    if (!el) return;
    if (isOnline) {
      el.innerHTML = `
        <div class="system-status-indicator" style="background: rgba(16, 185, 129, 0.08); border-color: rgba(16, 185, 129, 0.3); color: #047857; cursor: pointer;" onclick="platformApp.recheckConnection()" title="点击重新检测连接">
          <span class="pulse-dot"></span>
          <span>C++ 服务端已连接 (${latency}ms)</span>
        </div>
      `;
    } else {
      el.innerHTML = `
        <div class="system-status-indicator" style="background: rgba(99, 102, 241, 0.08); border-color: rgba(99, 102, 241, 0.25); color: #4338ca; cursor: pointer;" onclick="platformApp.recheckConnection()" title="C++ 服务端未连通，当前启用离线全真模拟沙箱，点击重试">
          <span class="badge-dot" style="background: #6366f1;"></span>
          <span>离线全真沙箱 (本地持久化)</span>
        </div>
      `;
    }
  }

  async recheckConnection() {
    this.showToast("正在探测 C++ 服务端 (127.0.0.1:8080/api/health)...", "info");
    const ok = await this.api.checkConnection();
    if (ok) {
      this.showToast("成功连接至本地 C++ 生产服务端！后续业务将直接走后端数据管道。", "success");
      await this.syncFromBackend();
    } else {
      this.showToast("服务端暂未启动或处于离线模式，已平滑无感切至本地高保真模拟数据引擎。", "warning");
    }
  }

  async syncFromBackend() {
    if (!this.api.isOnline) return;
    const backendRules = await this.api.getRules();
    if (backendRules && backendRules.length > 0) {
      this.rules = backendRules;
      this.saveRules();
    }
    const backendAlerts = await this.api.getAlerts();
    if (backendAlerts) {
      this.alerts = backendAlerts.map(a => ({
        id: a.id,
        studentId: a.student_id,
        studentName: a.student_name || "学生",
        class: a.class_name || "计科2101",
        ruleId: a.rule_id,
        ruleName: this.rules.find(r => r.id === a.rule_id)?.name || a.rule_id,
        level: a.level,
        status: a.status,
        metricValue: `${a.metric_value}`,
        created: new Date(a.created_at).getTime() || Date.now(),
        assignee: a.assignee || null,
        closedAt: null,
        interventions: a.interventions || []
      }));
      this.saveAlerts();
    }
    this.renderAll();
  }

  getCurrentStudent() {
    return this.students.find(s => s.id === this.currentStudentId) || this.students[0];
  }

  setStudent(studentId) {
    this.currentStudentId = studentId;
    localStorage.setItem("unigrowth_active_student", studentId);
    this.renderStudentProfile();
    this.renderCareerRecommendation();
  }

  setRole(role) {
    this.currentRole = role;
    localStorage.setItem("unigrowth_active_role", role);
    this.updateRoleBadges();
    this.showToast(`已切换当前登录视角：【${role}】`, "info");
  }

  toggleDesensitize() {
    this.isDesensitized = !this.isDesensitized;
    localStorage.setItem("unigrowth_desensitize", this.isDesensitized);
    this.renderStudentProfile();
    this.renderGovernance();
    this.updateDesensitizeButton();
    this.showToast(this.isDesensitized ? "《个人信息保护法》动态脱敏保护已启动" : "脱敏防护已临时解除（管理员明文调试）", "warning");
  }

  updateDesensitizeButton() {
    const btn = document.getElementById("top-desensitize-btn");
    if (btn) {
      btn.innerHTML = this.isDesensitized 
        ? `<svg viewBox="0 0 24 24" fill="none" stroke="#10b981" stroke-width="2" style="width:14px;height:14px;"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg><span>隐私脱敏已开启</span>` 
        : `<svg viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2" style="width:14px;height:14px;"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg><span>明文模式已开启</span>`;
    }
  }

  maskIdCard(idCard) {
    if (!this.isDesensitized || !idCard) return idCard;
    return idCard.replace(/^(\d{6})\d{8}(\w{4})$/, "$1********$2");
  }

  maskPhone(phone) {
    if (!this.isDesensitized || !phone) return phone;
    return phone.replace(/^(\d{3})\d{4}(\d{4})$/, "$1****$2");
  }

  formatTime(ts) {
    if (!ts) return "—";
    const d = new Date(ts);
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')} ${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
  }

  isOverdue(alert) {
    if (alert.status === "closed" || alert.status === "not_applicable") return false;
    return (Date.now() - alert.created) > 48 * 3600 * 1000;
  }

  // ===========================================================================
  // 1. 数据驾驶舱与多维群体分析 (Dashboard)
  // ===========================================================================
  async renderDashboard() {
    let studentCount = this.students.length;
    let activeAlerts = this.alerts.filter(a => a.status === "open" || a.status === "claimed").length;
    let overdueAlerts = this.alerts.filter(a => this.isOverdue(a)).length;
    let closedAlerts = this.alerts.filter(a => a.status === "closed").length;

    // 优先直连 C++ 后端聚合
    if (this.api.isOnline) {
      const data = await this.api.getDashboard(this.currentDashboardGroup);
      if (data) {
        studentCount = data.student_count || studentCount;
        activeAlerts = data.active_alerts !== undefined ? data.active_alerts : activeAlerts;
        overdueAlerts = data.overdue_alerts !== undefined ? data.overdue_alerts : overdueAlerts;
      }
    }

    const elTotal = document.getElementById("stat-total-students");
    const elActive = document.getElementById("stat-active-alerts");
    const elOverdue = document.getElementById("stat-overdue-alerts");
    const elClosed = document.getElementById("stat-closed-alerts");

    if (elTotal) animateCount(elTotal, 12480, 850);
    if (elActive) animateCount(elActive, activeAlerts, 650);
    if (elOverdue) animateCount(elOverdue, overdueAlerts, 650);
    if (elClosed) animateCount(elClosed, closedAlerts, 650);

    this.renderDashboardCharts();
  }

  setDashboardGroup(group) {
    this.currentDashboardGroup = group;
    const btns = document.querySelectorAll(".group-pill-btn");
    btns.forEach(b => {
      if (b.getAttribute("data-group") === group) b.classList.add("active");
      else b.classList.remove("active");
    });
    this.renderDashboardCharts();
  }

  renderDashboardCharts() {
    let barData = [];
    if (this.currentDashboardGroup === "major") {
      barData = [
        { label: "计算机科学与技术", value: 69.9, color: "#4f46e5" },
        { label: "软件工程", value: 67.9, color: "#06b6d4" }
      ];
    } else if (this.currentDashboardGroup === "grade") {
      barData = [
        { label: "2021级 (大四)", value: 69.2, color: "#4f46e5" },
        { label: "2022级 (大三)", value: 72.5, color: "#6366f1" },
        { label: "2023级 (大二)", value: 74.1, color: "#8b5cf6" },
        { label: "2024级 (大一)", value: 77.8, color: "#0ea5e9" }
      ];
    } else {
      // 默认按班级
      barData = [
        { label: "计科2101", value: 68.5, color: "#4f46e5" },
        { label: "计科2102", value: 71.3, color: "#6366f1" },
        { label: "软工2101", value: 74.8, color: "#0ea5e9" }
      ];
    }

    ModernChartEngine.renderBar("chart-dash-class-score", { data: barData, maxVal: 100 });

    const openCount = this.alerts.filter(a => a.status === "open").length;
    const claimedCount = this.alerts.filter(a => a.status === "claimed").length;
    const closedCount = this.alerts.filter(a => a.status === "closed").length;
    const naCount = this.alerts.filter(a => a.status === "not_applicable").length;

    ModernChartEngine.renderDonut("chart-dash-alerts-donut", {
      centerText: "预警全景",
      data: [
        { label: "待认领", value: openCount, color: "#ef4444" },
        { label: "跟进中", value: claimedCount, color: "#f59e0b" },
        { label: "已闭环", value: closedCount, color: "#10b981" },
        { label: "不适用", value: naCount, color: "#94a3b8" }
      ]
    });
  }

  // ===========================================================================
  // 2. 学业风险预警与规则引擎闭环 (Academic Alerts)
  // ===========================================================================
  async scanAndGenerateAlerts() {
    let generatedCount = 0;
    
    // 如果后端在线，真正调用 POST /api/modules/academic-alerts/alerts/generate
    if (this.api.isOnline) {
      const res = await this.api.generateAlerts();
      if (res) {
        generatedCount = res.generated || 0;
        await this.syncFromBackend();
        this.showToast(`C++ 服务端预警研判引擎执行完成：新增生成 ${generatedCount} 条风险，跳过已有 ${res.skipped || 0} 条。`, "success");
        return;
      }
    }

    // 本地沙箱模拟
    this.students.forEach(student => {
      this.rules.filter(r => r.enabled).forEach(rule => {
        let metricVal = 0;
        let isTriggered = false;

        if (rule.metric === "avg_score") {
          metricVal = student.avgScore;
          isTriggered = (rule.operator === "lt" && metricVal < rule.threshold) ||
                        (rule.operator === "lte" && metricVal <= rule.threshold);
        } else if (rule.metric === "failed_courses") {
          metricVal = student.failedCourses;
          isTriggered = (rule.operator === "gte" && metricVal >= rule.threshold) ||
                        (rule.operator === "gt" && metricVal > rule.threshold);
        } else if (rule.metric === "attendance") {
          metricVal = student.attendance;
          isTriggered = (rule.operator === "lt" && metricVal < rule.threshold) ||
                        (rule.operator === "lte" && metricVal <= rule.threshold);
        } else if (rule.metric === "credit_rate") {
          metricVal = student.creditRate;
          isTriggered = (rule.operator === "lt" && metricVal < rule.threshold) ||
                        (rule.operator === "lte" && metricVal <= rule.threshold);
        }

        if (isTriggered) {
          const exists = this.alerts.some(a => 
            a.studentId === student.id && 
            a.ruleId === rule.id && 
            (a.status === "open" || a.status === "claimed")
          );

          if (!exists) {
            const newAlert = {
              id: this.alerts.length > 0 ? Math.max(...this.alerts.map(a => a.id)) + 1 : 1,
              studentId: student.id,
              studentName: student.name,
              class: student.class,
              ruleId: rule.id,
              ruleName: rule.name,
              level: rule.level,
              status: "open",
              metricValue: rule.metric === 'failed_courses' ? `挂科 ${metricVal} 门` : (rule.metric === 'avg_score' ? `成绩 ${metricVal} 分` : `${(metricVal*100).toFixed(0)}%`),
              threshold: rule.threshold,
              created: Date.now(),
              assignee: null,
              closedAt: null,
              interventions: []
            };
            this.alerts.unshift(newAlert);
            generatedCount++;
          }
        }
      });
    });

    this.saveAlerts();
    this.renderAlertsModule();
    this.renderDashboard();

    if (generatedCount > 0) {
      this.showToast(`全域规则扫描完毕：新增研判出 ${generatedCount} 起待跟进学业风险！`, "warning");
    } else {
      this.showToast("全域规则研判完成：当前在管学生均处于稳定指标区间，未发现新增风险。", "success");
    }
  }

  async claimAlert(alertId) {
    const alert = this.alerts.find(a => a.id === alertId);
    if (!alert) return;

    if (alert.status !== "open") {
      this.showToast("该预警记录已被认领或处于归档状态", "warning");
      return;
    }

    if (this.api.isOnline) {
      await this.api.claimAlert(alertId);
    }

    alert.status = "claimed";
    alert.assignee = this.currentRole;
    alert.interventions.push({
      action: "claim",
      by: this.currentRole,
      operator: this.currentRole,
      at: new Date().toISOString(),
      time: Date.now(),
      note: `已由【${this.currentRole}】认领，转入重点帮扶跟进流程`
    });

    this.saveAlerts();
    this.renderAlertsModule();
    this.renderDashboard();
    this.showToast(`已成功认领学生【${alert.studentName}】的预警记录 #${alert.id}`, "success");
  }

  async submitHandleAlert(alertId, action, measures, note) {
    const alert = this.alerts.find(a => a.id === alertId);
    if (!alert) return { ok: false, msg: "预警记录不存在" };

    if (action === "close" && (!measures || !measures.trim())) {
      return { ok: false, msg: "闭环失败：必须如实填写帮扶与干预措施说明！" };
    }
    if (action === "not_applicable" && (!note || !note.trim())) {
      return { ok: false, msg: "标记不适用失败：必须填写原因备注说明！" };
    }

    const payload = {
      action: action,
      measures: measures ? measures.trim() : "",
      note: note ? note.trim() : "",
      by: this.currentRole
    };

    if (this.api.isOnline) {
      await this.api.handleAlert(alertId, payload);
    }

    if (action === "close") {
      alert.status = "closed";
      alert.closedAt = Date.now();
    } else if (action === "not_applicable") {
      alert.status = "not_applicable";
      alert.closedAt = Date.now();
    }

    alert.interventions.push({
      action: action,
      by: this.currentRole,
      operator: this.currentRole,
      at: new Date().toISOString(),
      time: Date.now(),
      measures: payload.measures,
      note: payload.note
    });

    this.saveAlerts();
    this.renderAlertsModule();
    this.renderDashboard();
    return { ok: true, msg: "学业风险处置结果已成功录入并归档！" };
  }

  renderAlertsModule() {
    const filterClass = document.getElementById("alert-filter-class")?.value || "all";
    const filterLevel = document.getElementById("alert-filter-level")?.value || "all";
    const filterStatus = document.getElementById("alert-filter-status")?.value || "all";
    const searchKeyword = (document.getElementById("alert-search-input")?.value || "").trim().toLowerCase();

    let filtered = this.alerts.filter(a => {
      if (filterClass !== "all" && a.class !== filterClass) return false;
      if (filterLevel !== "all" && a.level !== filterLevel) return false;
      if (filterStatus !== "all" && a.status !== filterStatus) return false;
      if (searchKeyword) {
        const matchName = a.studentName.toLowerCase().includes(searchKeyword);
        const matchId = a.studentId.includes(searchKeyword);
        const matchRule = a.ruleName.toLowerCase().includes(searchKeyword);
        if (!matchName && !matchId && !matchRule) return false;
      }
      return true;
    });

    const tbody = document.getElementById("alerts-table-body");
    if (!tbody) return;

    if (filtered.length === 0) {
      tbody.innerHTML = `<tr><td colspan="9" style="padding: 32px; text-align: center; color: var(--slate-400);">当前筛选条件下暂无风险预警记录</td></tr>`;
      return;
    }

    tbody.innerHTML = filtered.map(a => {
      const overdue = this.isOverdue(a);
      const levelBadge = a.level === "red" 
        ? `<span class="badge badge-danger"><span class="badge-dot" style="background:#ef4444;"></span>红色最高预警</span>`
        : (a.level === "orange" 
            ? `<span class="badge badge-warning"><span class="badge-dot" style="background:#f59e0b;"></span>橙色重点预警</span>` 
            : `<span class="badge" style="background:#fef9c3; color:#854d0e; border:1px solid #fde047;"><span class="badge-dot" style="background:#eab308;"></span>黄色关注预警</span>`);

      let statusBadge = "";
      if (a.status === "open") {
        statusBadge = overdue 
          ? `<span class="badge badge-danger" title="超48小时未处置"><svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>待认领 (逾期 &gt;48h)</span>`
          : `<span class="badge badge-danger" style="background:#fee2e2;">待认领</span>`;
      } else if (a.status === "claimed") {
        statusBadge = overdue 
          ? `<span class="badge badge-warning">跟进中 (处置超时)</span>`
          : `<span class="badge badge-warning">跟进中 (${a.assignee || '辅导员'})</span>`;
      } else if (a.status === "closed") {
        statusBadge = `<span class="badge badge-success"><svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="20 6 9 17 4 12"/></svg>已闭环</span>`;
      } else {
        statusBadge = `<span class="badge badge-slate">不适用</span>`;
      }

      let actionsHtml = "";
      if (a.status === "open") {
        actionsHtml = `
          <button class="btn btn-xs btn-primary" onclick="platformApp.claimAlert(${a.id})">认领</button>
          <button class="btn btn-xs btn-secondary" onclick="platformApp.openHandleModal(${a.id})">处置</button>
        `;
      } else if (a.status === "claimed") {
        actionsHtml = `
          <button class="btn btn-xs btn-warning" onclick="platformApp.openHandleModal(${a.id})">处置闭环</button>
        `;
      } else {
        actionsHtml = `
          <button class="btn btn-xs btn-secondary" onclick="platformApp.openAlertDetailModal(${a.id})">干预轨迹</button>
        `;
      }

      return `
        <tr>
          <td><strong class="font-mono text-muted">#${a.id}</strong></td>
          <td>
            <a href="javascript:void(0)" onclick="platformApp.jumpToStudent('${a.studentId}')" style="font-weight: 700; color: var(--slate-900); text-decoration: none;">
              ${a.studentName} <span class="font-mono text-xs text-muted">(${a.studentId})</span>
            </a>
          </td>
          <td>${a.class}</td>
          <td><strong>${a.ruleName}</strong></td>
          <td>${levelBadge}</td>
          <td><span class="font-mono font-bold">${a.metricValue}</span></td>
          <td>${statusBadge}</td>
          <td class="text-xs text-muted">${this.formatTime(a.created)}</td>
          <td><div class="d-flex gap-1">${actionsHtml}</div></td>
        </tr>
      `;
    }).join('');
  }

  jumpToStudent(studentId) {
    this.setStudent(studentId);
    const navLink = document.querySelector('[data-tab="student-profile"]');
    if (navLink) navLink.click();
  }

  openHandleModal(alertId) {
    const alert = this.alerts.find(a => a.id === alertId);
    if (!alert) return;

    const overdue = this.isOverdue(alert);
    const content = `
      <div style="display: flex; flex-direction: column; gap: 16px;">
        <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 10px; padding: 16px 20px;">
          <div style="display: flex; justify-content: space-between;">
            <span><strong>预警学生：</strong>${alert.studentName} (${alert.studentId} · ${alert.class})</span>
            <span><strong>触发规则：</strong>${alert.ruleName}</span>
          </div>
          <div style="font-size: 12px; color: var(--slate-500); margin-top: 8px;">
            <span>当前触发指标：<strong class="text-danger font-mono">${alert.metricValue}</strong></span>
            <span style="margin-left: 14px;">触发时间：${this.formatTime(alert.created)}</span>
            ${overdue ? '<span class="text-danger font-bold" style="margin-left: 14px;">⚠️ 预警逾期未结案 (&gt;48h)</span>' : ''}
          </div>
        </div>

        <div>
          <label style="font-size: 13px; font-weight: 700; color: var(--slate-900); display: block; margin-bottom: 8px;">
            选择处置动作：
          </label>
          <div style="display: flex; gap: 18px;">
            <label style="display: flex; align-items: center; gap: 6px; font-size: 13px; cursor: pointer;">
              <input type="radio" name="handle-action" value="close" checked onchange="platformApp.toggleHandleActionUI('close')">
              <strong>处置闭环 (Close)</strong>
            </label>
            <label style="display: flex; align-items: center; gap: 6px; font-size: 13px; cursor: pointer;">
              <input type="radio" name="handle-action" value="follow" onchange="platformApp.toggleHandleActionUI('follow')">
              <strong>过程谈心跟进 (Follow)</strong>
            </label>
            <label style="display: flex; align-items: center; gap: 6px; font-size: 13px; cursor: pointer;">
              <input type="radio" name="handle-action" value="not_applicable" onchange="platformApp.toggleHandleActionUI('not_applicable')">
              <strong>标记不适用 (N/A)</strong>
            </label>
          </div>
        </div>

        <div id="group-measures">
          <label style="font-size: 13px; font-weight: 700; color: var(--slate-900); display: block; margin-bottom: 6px;">
            帮扶干预措施记录 <span class="text-danger">*（必填，形成留痕审计）</span>：
          </label>
          <textarea id="modal-measures" class="form-textarea" rows="3" placeholder="例如：于2025年11月18日完成面对面深入谈心；安排班级学习委员一对一精准补习辅导；建立每周考勤打卡与课堂状态跟踪台账。"></textarea>
        </div>

        <div id="group-note" style="display: none;">
          <label style="font-size: 13px; font-weight: 700; color: var(--slate-900); display: block; margin-bottom: 6px;">
            不适用原因说明 <span class="text-danger">*（必填）</span>：
          </label>
          <textarea id="modal-note" class="form-textarea" rows="2" placeholder="例如：该生已于学期初办理休学/保留学籍手续，本学期不纳入学业考核评估。"></textarea>
        </div>

        <div style="display: flex; justify-content: flex-end; gap: 10px; margin-top: 10px;">
          <button class="btn btn-secondary" onclick="platformApp.closeModal()">取消</button>
          <button class="btn btn-primary" onclick="platformApp.submitHandleModal(${alert.id})">提交处置结果并留痕</button>
        </div>
      </div>
    `;

    this.openModal(`处置学业风险预警 #${alert.id}`, content);
  }

  toggleHandleActionUI(action) {
    const elMeasures = document.getElementById("group-measures");
    const elNote = document.getElementById("group-note");
    if (action === "close") {
      if (elMeasures) elMeasures.style.display = "block";
      if (elNote) elNote.style.display = "none";
    } else if (action === "not_applicable") {
      if (elMeasures) elMeasures.style.display = "none";
      if (elNote) elNote.style.display = "block";
    } else {
      if (elMeasures) elMeasures.style.display = "block";
      if (elNote) elNote.style.display = "none";
    }
  }

  async submitHandleModal(alertId) {
    const action = document.querySelector('input[name="handle-action"]:checked')?.value || "close";
    const measures = document.getElementById("modal-measures")?.value;
    const note = document.getElementById("modal-note")?.value;

    const result = await this.submitHandleAlert(alertId, action, measures, note);
    if (result.ok) {
      this.closeModal();
      this.showToast(result.msg, "success");
    } else {
      this.showToast(result.msg, "error");
    }
  }

  async openAlertDetailModal(alertId) {
    const alert = this.alerts.find(a => a.id === alertId);
    if (!alert) return;

    let interventions = alert.interventions || [];
    if (this.api.isOnline) {
      const remote = await this.api.getInterventions(alertId);
      if (remote && remote.length > 0) interventions = remote;
    }

    let timelineHtml = "";
    if (interventions.length === 0) {
      timelineHtml = `<div class="text-muted text-sm py-2">暂无干预过程流水记录</div>`;
    } else {
      timelineHtml = interventions.map(iv => `
        <div style="border-left: 2px solid var(--primary-300); padding-left: 16px; margin-left: 8px; margin-bottom: 16px; position: relative;">
          <div style="position: absolute; left: -7px; top: 0; width: 12px; height: 12px; border-radius: 50%; background: var(--primary); border: 2px solid #ffffff;"></div>
          <div class="d-flex align-center gap-2 mb-1">
            <strong>${iv.by || iv.operator || '辅导员'}</strong>
            <span class="badge badge-primary text-xs">${iv.action}</span>
            <span class="text-xs text-muted font-mono">${this.formatTime(iv.at ? new Date(iv.at).getTime() : iv.time)}</span>
          </div>
          <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 8px; padding: 10px 14px; font-size: 13px;">
            ${iv.measures ? `<div class="text-success"><strong>干预帮扶措施：</strong>${iv.measures}</div>` : ''}
            ${iv.note ? `<div class="text-muted"><strong>备注说明：</strong>${iv.note}</div>` : ''}
          </div>
        </div>
      `).join('');
    }

    const content = `
      <div style="display: flex; flex-direction: column; gap: 14px;">
        <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 10px; padding: 16px 20px; display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 13px;">
          <div>预警编号：<strong>#${alert.id}</strong></div>
          <div>预警学生：<strong>${alert.studentName} (${alert.studentId})</strong></div>
          <div>所属班级：<strong>${alert.class}</strong></div>
          <div>触发规则：<strong>${alert.ruleName}</strong></div>
          <div>预警等级：<span class="badge badge-${alert.level}">${alert.level}</span></div>
          <div>当前状态：<span class="badge badge-slate">${alert.status}</span></div>
          <div>触发时间：${this.formatTime(alert.created)}</div>
          <div>闭环时间：${this.formatTime(alert.closedAt)}</div>
        </div>

        <h4 style="font-size: 14px; font-weight: 800; color: var(--slate-900); margin-top: 10px;">帮扶干预全流程轨迹档案：</h4>
        <div>
          ${timelineHtml}
        </div>
      </div>
    `;

    this.openModal(`学业预警 #${alert.id} 全流程处置轨迹`, content);
  }

  // ===========================================================================
  // 3. 预警规则配置器中心 (Rule Configurator)
  // ===========================================================================
  openRuleConfigModal() {
    const rulesListHtml = this.rules.map(r => `
      <div style="display: flex; align-items: center; justify-content: space-between; padding: 12px 16px; border: 1px solid var(--border-light); border-radius: 8px; margin-bottom: 8px; background: #ffffff;">
        <div>
          <div style="font-weight: 700; color: var(--slate-900); display: flex; align-items: center; gap: 8px;">
            <span class="badge badge-slate font-mono">${r.id}</span>
            <span>${r.name}</span>
            <span class="badge badge-${r.level === 'red' ? 'danger' : (r.level === 'orange' ? 'warning' : 'slate')} text-xs">${r.level}</span>
          </div>
          <div style="font-size: 12px; color: var(--slate-500); margin-top: 4px;">
            判定指标：<code>${r.metric}</code> ${r.operator === 'lt' ? '<' : (r.operator === 'lte' ? '<=' : (r.operator === 'gte' ? '>=' : '>'))} <strong>${r.threshold}</strong> · 通知对象：${r.notify}
          </div>
        </div>
        <span class="badge badge-success">运行中</span>
      </div>
    `).join('');

    const content = `
      <div style="display: flex; flex-direction: column; gap: 18px;">
        <div>
          <h4 style="font-size: 13.5px; font-weight: 700; color: var(--slate-900); margin-bottom: 8px;">当前生产环境预警规则库：</h4>
          <div style="max-height: 200px; overflow-y: auto;">
            ${rulesListHtml}
          </div>
        </div>

        <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 10px; padding: 16px 20px;">
          <h4 style="font-size: 14px; font-weight: 800; color: var(--primary-600); margin-bottom: 12px;">+ 配置新增学业预警规则</h4>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
            <div>
              <label class="text-xs font-bold text-muted">规则名称：</label>
              <input id="rule-new-name" class="form-input" placeholder="例如：考勤严重不达标" value="重修门数预警">
            </div>
            <div>
              <label class="text-xs font-bold text-muted">监控指标 (Metric)：</label>
              <select id="rule-new-metric" class="form-select">
                <option value="avg_score">avg_score (平均学分绩点)</option>
                <option value="failed_courses" selected>failed_courses (不及格门数)</option>
                <option value="attendance">attendance (出勤率)</option>
                <option value="credit_rate">credit_rate (学分完成率)</option>
              </select>
            </div>
            <div>
              <label class="text-xs font-bold text-muted">比较操作符 (Operator)：</label>
              <select id="rule-new-op" class="form-select">
                <option value="gte" selected>&gt;= (大于等于)</option>
                <option value="gt">&gt; (大于)</option>
                <option value="lte">&lt;= (小于等于)</option>
                <option value="lt">&lt; (小于)</option>
              </select>
            </div>
            <div>
              <label class="text-xs font-bold text-muted">判定阈值 (Threshold)：</label>
              <input id="rule-new-thresh" class="form-input font-mono" type="number" step="0.1" value="3.0">
            </div>
            <div>
              <label class="text-xs font-bold text-muted">预警等级 (Level)：</label>
              <select id="rule-new-level" class="form-select">
                <option value="red" selected>红色最高预警</option>
                <option value="orange">橙色重点预警</option>
                <option value="yellow">黄色关注预警</option>
              </select>
            </div>
            <div>
              <label class="text-xs font-bold text-muted">通报范围 (Notify)：</label>
              <input id="rule-new-notify" class="form-input" value="辅导员、院系教务办">
            </div>
          </div>
          <div style="display: flex; justify-content: flex-end; margin-top: 14px;">
            <button class="btn btn-sm btn-primary" onclick="platformApp.submitCreateRule()">保存并部署至规则引擎</button>
          </div>
        </div>
      </div>
    `;

    this.openModal("学业风险规则引擎配置中心", content);
  }

  async submitCreateRule() {
    const name = document.getElementById("rule-new-name")?.value || "新建自定义规则";
    const metric = document.getElementById("rule-new-metric")?.value || "failed_courses";
    const op = document.getElementById("rule-new-op")?.value || "gte";
    const thresh = parseFloat(document.getElementById("rule-new-thresh")?.value || "3.0");
    const level = document.getElementById("rule-new-level")?.value || "red";
    const notify = document.getElementById("rule-new-notify")?.value || "辅导员";

    const newRule = {
      id: "R" + (this.rules.length + 1),
      name: name,
      metric: metric,
      operator: op,
      threshold: thresh,
      level: level,
      notify: notify,
      enabled: true
    };

    if (this.api.isOnline) {
      await this.api.createRule({
        name: newRule.name,
        metric: newRule.metric,
        operator: newRule.operator,
        threshold: newRule.threshold,
        level: newRule.level,
        notify: newRule.notify
      });
    }

    this.rules.push(newRule);
    this.saveRules();
    this.closeModal();
    this.showToast(`预警规则【${newRule.name}】已成功发布并在规则引擎中生效！`, "success");
  }

  // ===========================================================================
  // 4. 学生全息画像与档案 (Student Profile)
  // ===========================================================================
  renderStudentProfile() {
    const student = this.getCurrentStudent();
    if (!student) return;

    const nameEl = document.getElementById("prof-name");
    const idEl = document.getElementById("prof-id");
    const classEl = document.getElementById("prof-class");
    const majorEl = document.getElementById("prof-major");
    const idCardEl = document.getElementById("prof-idcard");
    const phoneEl = document.getElementById("prof-phone");
    const avgScoreEl = document.getElementById("prof-score");
    const failedEl = document.getElementById("prof-failed");
    const attendanceEl = document.getElementById("prof-attendance");
    const creditRateEl = document.getElementById("prof-credit");

    if (nameEl) nameEl.textContent = student.name;
    if (idEl) idEl.textContent = `学号: ${student.id}`;
    if (classEl) classEl.textContent = student.class;
    if (majorEl) majorEl.textContent = student.major;
    if (idCardEl) idCardEl.textContent = this.maskIdCard(student.idCard);
    if (phoneEl) phoneEl.textContent = this.maskPhone(student.phone);
    if (avgScoreEl) animateCount(avgScoreEl, student.avgScore, 700, true);
    if (failedEl) animateCount(failedEl, student.failedCourses, 600, false, " 门");
    if (attendanceEl) animateCount(attendanceEl, student.attendance * 100, 700, false, "%");
    if (creditRateEl) animateCount(creditRateEl, student.creditRate * 100, 700, false, "%");

    // 渲染雷达图
    const radarValues = [
      student.radar.academic,
      student.radar.project,
      student.radar.research,
      student.radar.comprehensive,
      student.radar.discipline,
      student.radar.teamwork
    ];
    const majorAvgValues = [72, 65, 58, 68, 82, 75];

    ModernChartEngine.renderRadar("chart-student-radar", {
      values: radarValues,
      benchmark: { name: "计算机专业年级均值", values: majorAvgValues }
    });

    // 渲染多学期折线图
    const trendData = student.trends.map(t => ({ label: t.term, value: t.score }));
    ModernChartEngine.renderLine("chart-student-trend", {
      data: trendData,
      minVal: 40,
      maxVal: 100
    });

    // 渲染标签云
    const tagsContainer = document.getElementById("prof-tags-container");
    if (tagsContainer) {
      tagsContainer.innerHTML = student.labels.map((lbl, idx) => `
        <div class="interactive-tag-pill" onclick="platformApp.showTagExplain(${idx})">
          <div class="tag-main-info">
            <div class="tag-badge-line">
              <span class="badge ${lbl.type === 'danger' ? 'badge-danger' : (lbl.type === 'warning' ? 'badge-warning' : (lbl.type === 'success' ? 'badge-success' : 'badge-primary'))}">
                ${lbl.text}
              </span>
              <span class="tag-explain-badge">
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                数据溯源
              </span>
            </div>
            <div class="tag-evidence-sub">${lbl.metric}</div>
          </div>
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--slate-400)" stroke-width="2"><polyline points="9 18 15 12 9 6"/></svg>
        </div>
      `).join('');
    }

    // 渲染修读课程列表
    const courseListEl = document.getElementById("prof-courses-list");
    if (courseListEl && student.courses) {
      courseListEl.innerHTML = student.courses.map(c => `
        <div class="course-item-row">
          <div class="course-name-wrap">
            <span class="course-title">${c.name}</span>
            <span class="course-meta">${c.code} · ${c.credits} 学分</span>
          </div>
          <div class="d-flex align-center gap-2">
            <strong class="${c.score < 60 ? 'text-danger' : 'text-primary'}" style="font-size: 14px;">${c.score} 分</strong>
            <span class="badge ${c.score < 60 ? 'badge-danger' : 'badge-slate'} text-xs">${c.status}</span>
          </div>
        </div>
      `).join('');
    }
  }

  showTagExplain(tagIdx) {
    const student = this.getCurrentStudent();
    if (!student || !student.labels[tagIdx]) return;
    const tag = student.labels[tagIdx];

    const content = `
      <div style="display: flex; flex-direction: column; gap: 14px;">
        <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 12px; padding: 16px 20px;">
          <div style="font-size: 11px; text-transform: uppercase; letter-spacing: 0.06em; color: var(--slate-400); font-weight: 700;">认证能力标签</div>
          <div style="font-size: 18px; font-weight: 800; color: var(--slate-900); margin-top: 3px;">
            ${tag.text}
          </div>
          <div style="font-size: 11.5px; color: var(--slate-500); margin-top: 4px;">评定计算时间：${tag.date} · 认证流水号：#TAG-CERT-${tagIdx + 101}</div>
        </div>

        <div style="display: grid; grid-template-columns: 1fr; gap: 12px; font-size: 13px;">
          <div style="display: flex; justify-content: space-between; border-bottom: 1px solid var(--border-subtle); padding-bottom: 10px;">
            <span style="color: var(--slate-500); font-weight: 600;">评定指标与数值</span>
            <span style="font-weight: 700; color: var(--slate-900); font-family: var(--font-mono);">${tag.metric}</span>
          </div>
          <div style="display: flex; justify-content: space-between; border-bottom: 1px solid var(--border-subtle); padding-bottom: 10px;">
            <span style="color: var(--slate-500); font-weight: 600;">计算公式与口径</span>
            <span style="font-weight: 700; color: var(--primary-600); font-family: var(--font-mono);">${tag.formula}</span>
          </div>
          <div style="display: flex; justify-content: space-between; border-bottom: 1px solid var(--border-subtle); padding-bottom: 10px;">
            <span style="color: var(--slate-500); font-weight: 600;">原始数据源通道</span>
            <span style="color: var(--slate-900); font-weight: 600;">${tag.source}</span>
          </div>
        </div>

        <div style="background: #ecfdf5; border: 1px solid #a7f3d0; border-radius: 10px; padding: 14px 18px; font-size: 12.5px; color: #065f46; line-height: 1.55;">
          <strong>评价公信力与数据穿透说明：</strong><br>
          该项评定结果通过中台统一主数据治理管道计算生成，所有原始成绩、考勤及项目记录均具备数字签名与不可篡改审计追踪链。
        </div>
      </div>
    `;
    this.openModal("能力画像标签可解释依据回溯", content);
  }

  exportStudentReport() {
    const student = this.getCurrentStudent();
    if (!student) return;

    const content = `
      <div style="display: flex; flex-direction: column; gap: 18px; padding: 4px;">
        <div style="text-align: center; border-bottom: 2px solid var(--primary-600); padding-bottom: 14px;">
          <h2 style="font-size: 19px; font-weight: 800; color: var(--slate-900);">高校学生学业成长与职业发展画像报告</h2>
          <div style="font-size: 12px; color: var(--slate-500); margin-top: 4px;">生成日期：${new Date().toLocaleDateString()} · 档案编号：UG-${student.id}</div>
        </div>

        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; font-size: 13px; background: var(--slate-50); padding: 16px; border-radius: 10px;">
          <div>姓名：<strong>${student.name}</strong> (${student.gender})</div>
          <div>学号：<strong>${student.id}</strong></div>
          <div>专业：<strong>${student.major}</strong></div>
          <div>班级：<strong>${student.class}</strong></div>
          <div>综合评定成绩：<strong class="text-primary">${student.avgScore.toFixed(1)} 分</strong></div>
          <div>学分修读进度：<strong>${(student.creditRate * 100).toFixed(0)}%</strong></div>
        </div>

        <div>
          <h4 style="font-size: 13.5px; font-weight: 700; color: var(--slate-900); margin-bottom: 10px;">认证能力素养综合评定：</h4>
          <div style="display: flex; flex-wrap: wrap; gap: 8px;">
            ${student.labels.map(l => `<span class="badge badge-slate">${l.text} (${l.metric})</span>`).join('')}
          </div>
        </div>

        <div style="display: flex; justify-content: flex-end; gap: 10px; margin-top: 10px;">
          <button class="btn btn-secondary" onclick="platformApp.closeModal()">关闭</button>
          <button class="btn btn-primary" onclick="window.print()">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:14px;height:14px;"><polyline points="6 9 6 2 18 2 18 9"/><path d="M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2"/><rect x="6" y="14" width="12" height="8"/></svg>
            <span>打印 / 导出 PDF</span>
          </button>
        </div>
      </div>
    `;

    this.openModal(`成长档案报告导出 - ${student.name}`, content);
  }

  // ===========================================================================
  // 5. 职业发展推荐与人岗匹配 (Career Intelligence)
  // ===========================================================================
  renderCareerRecommendation() {
    const student = this.getCurrentStudent();
    if (!student) return;

    const listEl = document.getElementById("career-jobs-list");
    if (!listEl) return;

    const matches = JOBS_DATA.map(job => {
      let matchScore = 0;
      let matchedItems = [];
      let gapItems = [];

      job.abilities.forEach(ab => {
        let studentSkill = 70;
        if (ab.name.includes("C++") || ab.name.includes("系统")) {
          studentSkill = student.radar.project * 0.7 + student.radar.academic * 0.3;
        } else if (ab.name.includes("算法") || ab.name.includes("数学")) {
          studentSkill = student.radar.research * 0.7 + student.radar.academic * 0.3;
        } else if (ab.name.includes("Web") || ab.name.includes("前端")) {
          studentSkill = student.radar.project * 0.8 + student.radar.teamwork * 0.2;
        } else if (ab.name.includes("Java") || ab.name.includes("数据库")) {
          studentSkill = student.radar.project * 0.6 + student.radar.academic * 0.4;
        }

        const isMet = studentSkill >= ab.reqScore;
        const contrib = Math.min(100, (studentSkill / ab.reqScore) * 100) * ab.weight;
        matchScore += contrib;

        if (isMet) {
          matchedItems.push({ name: ab.name, score: Math.round(studentSkill), req: ab.reqScore });
        } else {
          gapItems.push({ name: ab.name, score: Math.round(studentSkill), req: ab.reqScore });
        }
      });

      matchScore = Math.min(98, Math.max(45, Math.round(matchScore)));
      return { job, matchScore, matchedItems, gapItems };
    });

    matches.sort((a, b) => b.matchScore - a.matchScore);

    listEl.innerHTML = matches.map(m => `
      <div class="career-job-card">
        <div>
          <div class="job-card-header">
            <div class="job-title-group">
              <h4>${m.job.name}</h4>
              <div class="job-industry-tag">${m.job.industry} · ${m.job.degree}</div>
            </div>
            <div class="job-match-gauge">
              <div class="job-match-percent ${m.matchScore >= 85 ? 'text-success' : 'text-primary'}">${m.matchScore}%</div>
              <div class="job-match-lbl">人岗适配度</div>
            </div>
          </div>
          
          <p style="font-size: 13px; color: var(--slate-600); margin-bottom: 16px; line-height: 1.5;">${m.job.desc}</p>
          
          <div class="job-skills-section">
            <div class="job-skills-heading">岗位能力胜任力诊断：</div>
            <div class="job-pills-wrap">
              ${m.matchedItems.map(item => `<span class="skill-pill-strength">契合优势: ${item.name} (${item.score}/${item.req})</span>`).join('')}
              ${m.gapItems.map(item => `<span class="skill-pill-gap">建议强化: ${item.name} (${item.score}/${item.req})</span>`).join('')}
            </div>
          </div>
        </div>

        <div style="display: flex; justify-content: space-between; align-items: center; border-top: 1px solid var(--border-light); padding-top: 16px; margin-top: 12px;">
          <span style="font-size: 14px; font-weight: 800; color: var(--primary-600); font-family: var(--font-mono);">${m.job.salary}</span>
          <div class="d-flex gap-1">
            <button class="btn btn-xs btn-outline" onclick="platformApp.feedbackJob('${m.job.id}', 'interested')">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:12px;height:12px;"><polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/></svg>
              <span>标记意向</span>
            </button>
            <button class="btn btn-xs btn-secondary" onclick="platformApp.feedbackJob('${m.job.id}', 'not_interested')">暂不考虑</button>
          </div>
        </div>
      </div>
    `).join('');
  }

  feedbackJob(jobId, action) {
    const job = JOBS_DATA.find(j => j.id === jobId);
    const msg = action === "interested" 
      ? `已标记【${job.name}】为学生求职意向岗位，系统已优化推荐流权重！` 
      : `已记录反馈，后续将降低同类职位的推荐频次。`;
    this.showToast(msg, "info");
  }

  // ===========================================================================
  // 6. 数据资产与主数据治理 (Data Governance)
  // ===========================================================================
  renderGovernance() {
    const tbody = document.getElementById("master-students-table-body");
    if (!tbody) return;

    tbody.innerHTML = this.students.map(s => `
      <tr>
        <td><strong class="font-mono text-dark">${s.id}</strong></td>
        <td><strong>${s.name}</strong></td>
        <td><span class="font-mono text-xs">${this.maskIdCard(s.idCard)}</span></td>
        <td>${s.class}</td>
        <td>${s.major}</td>
        <td><span class="badge badge-success">一人一档已融合</span></td>
        <td><span class="badge badge-slate font-mono">${s.avgScore.toFixed(1)} 分</span></td>
        <td><span class="text-xs text-muted">教务/科研/学工 (3源)</span></td>
      </tr>
    `).join('');
  }

  // ===========================================================================
  // 7. 数据服务开放与在线 API 测试沙箱 (Data Services)
  // ===========================================================================
  renderDataServices() {
    const listEl = document.getElementById("services-catalog-cards");
    if (!listEl) return;

    listEl.innerHTML = DATA_SERVICES_CATALOG.map(svc => `
      <div class="api-service-card">
        <div class="api-card-top">
          <div>
            <div style="font-size: 15px; font-weight: 800; color: var(--slate-900);">${svc.name}</div>
            <div class="api-endpoint-badge" style="margin-top: 5px;">
              <span class="method-tag ${svc.method === 'GET' ? 'method-get' : 'method-post'}">${svc.method}</span>
              <span>${svc.path}</span>
            </div>
          </div>
          <span class="badge badge-slate">${svc.version}</span>
        </div>
        <p style="font-size: 12.5px; color: var(--slate-600); margin: 8px 0; line-height: 1.5;">${svc.desc}</p>
        <div style="display: flex; justify-content: space-between; align-items: center; border-top: 1px solid var(--border-subtle); margin-top: 10px; padding-top: 10px; font-size: 12px;">
          <span class="text-muted">授权范围：<strong>${svc.scope}</strong> · 限流：<strong>${svc.qpsLimit} QPS</strong></span>
          <div class="d-flex gap-1">
            <button class="btn btn-xs btn-outline" onclick="platformApp.testApiInSandbox('${svc.path}', '${svc.method}')">
              在线测试
            </button>
            <button class="btn btn-xs btn-primary" onclick="platformApp.applyServiceToken('${svc.id}')">
              申请凭证 Token
            </button>
          </div>
        </div>
      </div>
    `).join('');
  }

  async testApiInSandbox(path, method = "GET") {
    const url = path.replace(":alert_id", "1").replace(":student_id", "2021001");
    this.showToast(`正在向网关发送实时请求：${method} ${url}`, "info");

    let statusText = "200 OK (模拟沙箱响应)";
    let jsonResult = {};

    try {
      if (this.api.isOnline) {
        const res = await fetch(`${this.api.baseUrl}${url}`);
        statusText = `${res.status} ${res.statusText}`;
        jsonResult = await res.json();
      } else {
        // 沙箱响应
        if (url.includes("/dashboard")) {
          jsonResult = { group_by: "class", student_count: 12, avg_score: 66.8, active_alerts: this.alerts.filter(a => a.status === 'open' || a.status === 'claimed').length, overdue_alerts: 1 };
        } else if (url.includes("/alerts")) {
          jsonResult = { total: this.alerts.length, alerts: this.alerts.slice(0, 3) };
        } else {
          jsonResult = { status: "ok", timestamp: Date.now(), client: "unigrowth-sandbox" };
        }
      }
    } catch (e) {
      statusText = "500 Error";
      jsonResult = { error: e.message };
    }

    const content = `
      <div style="display: flex; flex-direction: column; gap: 14px;">
        <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 8px; padding: 12px 16px; font-family: var(--font-mono); font-size: 13px;">
          <span class="badge ${method === 'GET' ? 'badge-primary' : 'badge-success'}">${method}</span>
          <strong style="margin-left: 8px;">${url}</strong>
          <span class="badge badge-success ml-auto" style="float: right;">${statusText}</span>
        </div>

        <div>
          <label style="font-size: 12px; font-weight: 700; color: var(--slate-600); margin-bottom: 6px; display: block;">HTTP Response Body (JSON):</label>
          <pre style="background: var(--slate-50); color: #0f172a; border: 1px solid var(--border-card); padding: 16px; border-radius: 8px; font-size: 12px; max-height: 280px; overflow-y: auto; line-height: 1.5; font-family: var(--font-mono);">${JSON.stringify(jsonResult, null, 2)}</pre>
        </div>

        <div style="display: flex; justify-content: flex-end;">
          <button class="btn btn-secondary" onclick="platformApp.closeModal()">关闭</button>
        </div>
      </div>
    `;

    this.openModal(`数据服务接口响应沙箱 · ${path}`, content);
  }

  applyServiceToken(svcId) {
    const svc = DATA_SERVICES_CATALOG.find(s => s.id === svcId);
    if (!svc) return;

    const mockToken = "Bearer unigrowth_" + Math.random().toString(36).substring(2, 12) + "." + Math.random().toString(36).substring(2, 10);
    const content = `
      <div style="display: flex; flex-direction: column; gap: 14px;">
        <div style="background: var(--slate-50); border: 1px solid var(--border-light); border-radius: 10px; padding: 16px 20px;">
          <div style="font-size: 11px; text-transform: uppercase; color: var(--slate-400); font-weight: 700;">开放数据服务</div>
          <div style="font-size: 16px; font-weight: 800; color: var(--slate-900);">${svc.name}</div>
          <div style="font-size: 12px; font-family: var(--font-mono); color: var(--primary-600); margin-top: 3px;">${svc.path}</div>
        </div>

        <div>
          <label style="font-size: 13px; font-weight: 700; color: var(--slate-900); display: block; margin-bottom: 6px;">
            授权访问凭证令牌 (Bearer Token)：
          </label>
          <textarea class="form-textarea font-mono text-xs" rows="2" readonly style="background: var(--slate-50);">${mockToken}</textarea>
        </div>

        <div style="font-size: 12px; color: var(--slate-500); line-height: 1.6;">
          * 令牌经数据中心审批签发，有效期 7 天。系统已自动在安全审计流水中留痕。
        </div>

        <div style="display: flex; justify-content: flex-end;">
          <button class="btn btn-primary" onclick="platformApp.closeModal()">完成确认</button>
        </div>
      </div>
    `;

    this.openModal(`数据服务授权凭证 - ${svc.name}`, content);
  }

  runIngestionTask(sourceName) {
    const modalContent = `
      <div style="display: flex; flex-direction: column; gap: 16px; padding: 10px 0;">
        <p style="font-size: 13.5px; color: var(--slate-700);">
          正在连接外部数据源 <strong>${sourceName}</strong> 执行增量同步…
        </p>
        <div style="width: 100%; height: 8px; background: var(--slate-200); border-radius: 4px; overflow: hidden;">
          <div id="ingest-progress" style="width: 0%; height: 100%; background: linear-gradient(90deg, #4f46e5, #10b981); transition: width 0.3s ease;"></div>
        </div>
        <div id="ingest-log" class="font-mono text-xs" style="background: var(--slate-50); color: #0f172a; border: 1px solid var(--border-card); padding: 14px; border-radius: 8px; max-height: 160px; overflow-y: auto; line-height: 1.6;">
          [0.0s] 握手成功，读取增量数据批次...<br>
        </div>
      </div>
    `;
    this.openModal(`外部数据源增量接入 · ${sourceName}`, modalContent);

    let progress = 0;
    const progressEl = document.getElementById("ingest-progress");
    const logEl = document.getElementById("ingest-log");

    const timer = setInterval(() => {
      progress += 25;
      if (progressEl) progressEl.style.width = `${progress}%`;

      if (progress === 25 && logEl) {
        logEl.innerHTML += `[0.4s] 验证源字段映射标准（学号/课程/成绩/打卡）...通过<br>`;
      } else if (progress === 50 && logEl) {
        logEl.innerHTML += `[0.8s] 执行数据质量核验（完整性 99.8% / 唯一性 100%）...通过<br>`;
      } else if (progress === 75 && logEl) {
        logEl.innerHTML += `[1.2s] 写入主数据湖，触发学生成长画像指标刷新...<br>`;
      } else if (progress >= 100) {
        clearInterval(timer);
        if (logEl) {
          logEl.innerHTML += `[1.5s] 增量同步完成！融合 1,248 条记录，数据中台指标已同步生效。<br>`;
        }
        this.showToast(`数据源【${sourceName}】增量同步成功！`, "success");
      }
    }, 400);
  }

  // ===========================================================================
  // 通用模态与 Toast
  // ===========================================================================
  openModal(title, bodyHtml) {
    const modal = document.getElementById("app-modal");
    const titleEl = document.getElementById("app-modal-title");
    const bodyEl = document.getElementById("app-modal-body");

    if (titleEl) titleEl.textContent = title;
    if (bodyEl) bodyEl.innerHTML = bodyHtml;
    if (modal) modal.classList.add("active");
  }

  closeModal() {
    const modal = document.getElementById("app-modal");
    if (modal) modal.classList.remove("active");
  }

  showToast(message, type = "info") {
    const container = document.getElementById("toast-container");
    if (!container) return;

    const toast = document.createElement("div");
    toast.className = `toast toast-${type}`;
    
    let iconSvg = "";
    if (type === "success") {
      iconSvg = `<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#10b981" stroke-width="2"><polyline points="20 6 9 17 4 12"/></svg>`;
    } else if (type === "error") {
      iconSvg = `<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>`;
    } else if (type === "warning") {
      iconSvg = `<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#f59e0b" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>`;
    } else {
      iconSvg = `<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#0ea5e9" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>`;
    }

    toast.innerHTML = `
      ${iconSvg}
      <span>${message}</span>
    `;

    container.appendChild(toast);
    setTimeout(() => {
      toast.classList.add("fade-out");
      setTimeout(() => toast.remove(), 260);
    }, 3200);
  }

  updateRoleBadges() {
    const rolePill = document.getElementById("sidebar-user-role");
    if (rolePill) rolePill.textContent = this.currentRole;
  }

  bindEvents() {
    const modal = document.getElementById("app-modal");
    if (modal) {
      modal.addEventListener("click", (e) => {
        if (e.target === modal) this.closeModal();
      });
    }
  }

  renderAll() {
    this.updateRoleBadges();
    this.updateDesensitizeButton();
    this.renderDashboard();
    this.renderStudentProfile();
    this.renderAlertsModule();
    this.renderCareerRecommendation();
    this.renderGovernance();
    this.renderDataServices();
  }
}

window.PlatformApp = PlatformApp;


/**
 * 高校学生成长与职业发展数据中台 - 商业级主路由与交互控制器 (App Router)
 * 纯业务管理平台：数据驾驶舱、学生全息画像、学业预警处置、职业发展推荐、数据资产治理、数据服务开放
 */

document.addEventListener("DOMContentLoaded", () => {
  const platformApp = new PlatformApp();
  window.platformApp = platformApp;

  // 1. 侧边栏导航 Tab 切换
  const navLinks = document.querySelectorAll(".sidebar-link[data-tab]");
  const tabPanes = document.querySelectorAll(".tab-pane");
  const pageTitleEl = document.getElementById("topbar-page-title");
  const pageBreadcrumbEl = document.getElementById("topbar-breadcrumb");

  const TAB_TITLES = {
    "dashboard": { title: "数据驾驶舱", sub: "全校学情态势 / 核心运营指标大屏" },
    "student-profile": { title: "学生全息成长画像", sub: "一人一档 / 六维能力素养雷达 / 学业轨迹" },
    "academic-alerts": { title: "学业风险预警与干预中心", sub: "多维风险监控 / 谈心谈话帮扶 / 处置闭环管理" },
    "career-rec": { title: "职业发展与智能推荐", sub: "行业岗位画像匹配 / 适配度诊断 / 技能短板分析" },
    "data-governance": { title: "数据资产与主数据治理", sub: "异构多源接入 / 一人一档主数据湖 / 质量治理" },
    "data-services": { title: "数据开放与安全网关", sub: "标准 Open API 目录 / 访问授权 / 隐私脱敏合规" }
  };

  function switchTab(targetTabId) {
    if (!document.getElementById(`tab-${targetTabId}`)) {
      targetTabId = "dashboard";
    }

    navLinks.forEach(link => {
      if (link.getAttribute("data-tab") === targetTabId) {
        link.classList.add("active");
      } else {
        link.classList.remove("active");
      }
    });

    tabPanes.forEach(pane => {
      if (pane.id === `tab-${targetTabId}`) {
        pane.classList.add("active");
      } else {
        pane.classList.remove("active");
      }
    });

    if (TAB_TITLES[targetTabId]) {
      if (pageTitleEl) pageTitleEl.textContent = TAB_TITLES[targetTabId].title;
      if (pageBreadcrumbEl) pageBreadcrumbEl.textContent = TAB_TITLES[targetTabId].sub;
    }

    // 触发图表自适应重绘与动效展开
    if (targetTabId === "dashboard") {
      setTimeout(() => platformApp.renderDashboard(), 50);
    } else if (targetTabId === "student-profile") {
      setTimeout(() => platformApp.renderStudentProfile(), 50);
    }

    window.location.hash = targetTabId;
    window.scrollTo({ top: 0, behavior: 'smooth' });
  }

  navLinks.forEach(link => {
    link.addEventListener("click", (e) => {
      e.preventDefault();
      const tabId = link.getAttribute("data-tab");
      switchTab(tabId);
    });
  });

  // 2. Hash 路由恢复
  const currentHash = window.location.hash.replace("#", "");
  if (currentHash && document.getElementById(`tab-${currentHash}`)) {
    switchTab(currentHash);
  } else {
    switchTab("dashboard");
  }

  // 3. 学生选择下拉框联动 (12 名在册学员)
  const studentSelect = document.getElementById("profile-student-select");
  if (studentSelect) {
    studentSelect.innerHTML = STUDENTS_MASTER.map(s => `
      <option value="${s.id}" ${s.id === platformApp.currentStudentId ? 'selected' : ''}>
        ${s.name} (${s.id} · ${s.class} · 绩点 ${s.avgScore})
      </option>
    `).join('');

    studentSelect.addEventListener("change", (e) => {
      platformApp.setStudent(e.target.value);
    });
  }

  // 4. 身份切换联动
  const topRoleSelect = document.getElementById("top-role-select");
  if (topRoleSelect) {
    topRoleSelect.value = platformApp.currentRole;
    topRoleSelect.addEventListener("change", (e) => {
      platformApp.setRole(e.target.value);
    });
  }

  // 5. 预警列表筛选与关键字监听
  const alertClass = document.getElementById("alert-filter-class");
  const alertLevel = document.getElementById("alert-filter-level");
  const alertStatus = document.getElementById("alert-filter-status");
  const alertSearch = document.getElementById("alert-search-input");

  [alertClass, alertLevel, alertStatus].forEach(el => {
    if (el) el.addEventListener("change", () => platformApp.renderAlertsModule());
  });

  if (alertSearch) {
    alertSearch.addEventListener("input", () => platformApp.renderAlertsModule());
  }

  // 班级快捷筛选 Pill 按钮
  const classPills = document.querySelectorAll(".filter-pill-btn[data-class]");
  classPills.forEach(btn => {
    btn.addEventListener("click", () => {
      classPills.forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      const cls = btn.getAttribute("data-class");
      if (alertClass) alertClass.value = cls;
      platformApp.renderAlertsModule();
    });
  });

  // 6. 全局初始化渲染
  platformApp.renderAll();

  // 7. 鼠标交互聚光灯光晕追踪 (Linear / Vercel 光标跟随动效)
  document.addEventListener("mousemove", (e) => {
    const card = e.target.closest(".pro-card, .metric-tile, .career-job-card, .pipeline-node, .student-hero-card, .datasource-card, .api-service-card");
    if (card) {
      const rect = card.getBoundingClientRect();
      card.style.setProperty("--mouse-x", `${e.clientX - rect.left}px`);
      card.style.setProperty("--mouse-y", `${e.clientY - rect.top}px`);
    }
  });
});

