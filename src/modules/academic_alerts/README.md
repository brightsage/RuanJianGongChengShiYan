# M4 学业分析与预警 · 模块说明

> **负责人**：刘家庚（分支 `feature/LiuJiaGeng`）
> **路由前缀**：`/api/modules/academic-alerts/`
> **需求依据**：《实验一 需求分析与迭代规划》§2.3.5（用例 UC-AA-01～UC-AA-07）、§3.3（关键用例规约）、§5（非功能需求）
> **代码位置**：`src/modules/academic_alerts/module.cpp`

---

## 1. 模块定位

本模块负责学生学业风险的**发现、处置、跟踪**闭环，是数据中台「分析—预警」能力面向辅导员与院系管理员的出口。

```
M2 指标值（综合成绩、挂科门数、学分完成率、出勤率）
        │
        ▼
   M4 规则判定 ──► 分级预警名单 ──► 辅导员认领 / 谈话 / 记录 ──► 关闭 or 跟踪
```

- **上游依赖**：M2 数据治理产出的指标值（当前用内存种子数据模拟，接口已按指标名解耦）
- **下游消费**：M3 画像可引用预警状态作为标签；M6 数据服务可将预警统计封装为对外 API

---

## 2. 需求追溯

| 用例 | 需求描述 | 实现接口 | 关键规则 |
| --- | --- | --- | --- |
| UC-AA-01 查看学业看板 | 班级/专业/年级的学业指标概览与分布 | `GET /dashboard` | 支持 `class`、`major` 过滤 |
| UC-AA-02 群体分析 | 按维度下钻对比 | `GET /dashboard?group=` | `class` / `major` / `grade` |
| UC-AA-03 配置预警规则 | 设置触发条件、阈值、等级与通知对象 | `GET /rules`、`POST /rules` | 指标、运算符、阈值、等级四要素校验 |
| UC-AA-04 生成预警名单 | 按规则周期计算并生成分级名单 | `POST /alerts/generate` | 幂等：同学生同规则存在未关闭预警时不重复生成 |
| | 查看预警名单 | `GET /alerts` | 支持 `status`、`level`、`class` 过滤 |
| UC-AA-05 处置预警 | 认领、记录措施、更新状态 | `POST /alerts/:id/claim`、`POST /alerts/:id/handle` | 越权不可认领；未填措施不得关闭 |
| UC-AA-06 跟踪干预记录 | 查看干预历史、超时提醒 | `GET /alerts/:id/interventions` | 超过 48 小时未处置标记 `overdue` |
| UC-AA-07 分析学业趋势 | 多学期趋势分析 | `GET /students/:id/trend` | 按学期返回综合成绩 |

---

## 3. 预警状态机

```
                认领 claim
   [open] ─────────────────► [claimed]
      │                          │
      │ 标记不适用                │ 处置 close（必须填 measures）
      │ not_applicable           ▼
      │（必须填 note）         [closed]  ← 历史不可修改
      ▼
 [not_applicable]
```

| 状态 | 含义 | 可执行动作 |
| --- | --- | --- |
| `open` | 已生成，等待认领 | claim、handle(follow/close/not_applicable) |
| `claimed` | 已被辅导员认领 | handle(follow/close/not_applicable) |
| `closed` | 已闭环（填写了干预措施） | 只读，重复处置返回 409 |
| `not_applicable` | 转专业 / 休学等，不适用 | 只读 |

---

## 4. 接口清单

### 4.1 学业看板 / 群体分析

```
GET /api/modules/academic-alerts/dashboard?group=class&class=计科2101
```

```json
{
  "group_by": "class",
  "student_count": 4,
  "avg_score": 64.03,
  "failing_students": 3,
  "active_alerts": 8,
  "overdue_alerts": 0,
  "distribution": { "计科2101": 4 }
}
```

### 4.2 预警规则

```
GET  /api/modules/academic-alerts/rules
POST /api/modules/academic-alerts/rules
Body: {"name":"出勤率过低","metric":"attendance","operator":"lt","threshold":0.6,"level":"red","notify":"辅导员"}
```

- `metric`：`avg_score` | `failed_courses` | `credit_rate` | `attendance`
- `operator`：`lt` | `lte` | `gt` | `gte`
- `level`：`red` | `orange` | `yellow`

### 4.3 生成预警名单（幂等）

```
POST /api/modules/academic-alerts/alerts/generate
```

```json
{ "generated": 20, "skipped": 1, "total_alerts": 21 }
```

`skipped` 表示命中规则但已存在未关闭预警，未重复生成。

### 4.4 预警列表

```
GET /api/modules/academic-alerts/alerts?status=open&level=red&class=计科2101
```

返回 `alerts` 数组，每条含 `id / student_id / student_name / class_name / rule_id / level / status / created_at / assignee / metric_value / overdue / interventions`。

### 4.5 认领与处置

```
POST /api/modules/academic-alerts/alerts/2/claim
Body: {"assignee":"王老师"}

POST /api/modules/academic-alerts/alerts/2/handle
Body: {"action":"close","measures":"每周两次答疑，已联系家长","note":"成绩回升中","by":"王老师"}
```

### 4.6 干预记录

```
GET /api/modules/academic-alerts/alerts/2/interventions
```

```json
{
  "alert_id": 2,
  "status": "closed",
  "overdue": false,
  "records": [
    {"at":"2026-10-27T07:41:40Z","action":"claim","measures":"","note":"认领预警","by":"王老师"},
    {"at":"2026-10-27T07:41:40Z","action":"close","measures":"每周两次答疑，已联系家长","note":"成绩回升中","by":"王老师"}
  ]
}
```

### 4.7 学业趋势

```
GET /api/modules/academic-alerts/students/2021007/trend
```

```json
{
  "student_id": "2021007",
  "name": "周杰",
  "class_name": "计科2102",
  "terms": [
    {"term":"2024-2025-1","avg_score":52.3},
    {"term":"2024-2025-2","avg_score":49.1},
    {"term":"2025-2026-1","avg_score":47.8}
  ]
}
```

---

## 5. 错误码约定

| 状态码 | 场景 | 示例 |
| --- | --- | --- |
| 400 | 参数缺失或取值非法 | 未填 `measures` 就关闭；`metric=gpa` |
| 404 | 学生/预警/路由不存在 | `GET /students/9999/trend` |
| 409 | 状态冲突 | 已被他人认领；重复关闭 |
| 200/201 | 成功 / 新建成功 | — |

业务错误统一返回 `{"error":"中文原因"}`，便于前端直接提示。

---

## 6. 数据模型（当前为内存实现）

| 结构 | 主要字段 | 说明 |
| --- | --- | --- |
| `Student` | `id`、`name`、`class_name`、`major`、`grade`、`avg_score`、`failed_courses`、`credit_rate`、`attendance`、`trend[]` | 12 名种子学生、3 个班级 |
| `Rule` | `id`、`name`、`metric`、`op`、`threshold`、`level`、`notify`、`enabled` | 默认 R1～R4 |
| `Alert` | `id`、`student_id`、`rule_id`、`level`、`status`、`created`、`assignee`、`metric_value`、`interventions[]` | 触发时快照指标值 |
| `Intervention` | `at`、`action`、`measures`、`note`、`by` | 认领与每次处置都留痕 |

数据放在 `store()` 的函数内静态变量里，进程重启即重置——这是实验阶段的刻意选择，接口层已与存储解耦，替换为数据库时只需改 `store()` 的读写。

---

## 7. 测试

### 7.1 自动化

```bash
ctest --test-dir build --output-on-failure
# 100% tests passed, 0 tests failed out of 2
```

- `platform_tests`：应用层路由注册、匹配、404 行为
- `http_smoke`：真实 HTTP 边界（健康检查、模块目录、静态页、请求体超限）

### 7.2 关键手工用例（在浏览器测试台执行）

| 编号 | 场景 | 输入 | 预期 |
| --- | --- | --- | --- |
| TC-01 | 生成预警 | `POST /alerts/generate` | 200，`generated=20`，`skipped=1` |
| TC-02 | 幂等 | 再次生成 | 200，`generated=0` |
| TC-03 | 等级筛选 | `GET /alerts?level=red` | 200，返回 4 条红色预警 |
| TC-04 | 认领 | `{"assignee":"王老师"}` | 200，`status=claimed` |
| TC-05 | 越权认领 | 换 `{"assignee":"李老师"}` | **409** |
| TC-06 | 未填措施关闭 | `{"action":"close"}` | **400** |
| TC-07 | 正常关闭 | 带 `measures` | 200，`status=closed` |
| TC-08 | 重复关闭 | 同 TC-07 | **409** |
| TC-09 | 不适用需原因 | `{"action":"not_applicable"}` | **400**，补 `note` 后 200 |
| TC-10 | 非法指标 | `POST /rules`，`metric=gpa` | **400** |
| TC-11 | 未知学生 | `GET /students/9999/trend` | **404** |
| TC-12 | 阈值边界 | 新增 `avg_score lte 58.4` | 张伟（58.4）被触发；`lt 58.4` 不触发 |
| TC-13 | 超时提醒 | `GET /alerts?status=open` | 种子预警 `overdue=true` |

---

## 8. 已知边界与后续计划

| 项 | 现状 | 后续 |
| --- | --- | --- |
| 数据存储 | 内存静态变量，重启丢失 | 接 MySQL 或其他持久化 |
| 指标来源 | 种子数据直接给出综合成绩等 | 从 M2 的指标值表读取，阈值改为读指标口径配置 |
| 权限 | 认领只做「先到先得」 | 接 M6 的角色与数据范围校验（辅导员只能处置所辖班级） |
| 通知 | 仅记录 `notify` 字段 | 接消息推送（站内信/邮件） |
| 并发 | 单线程服务 | 平台本身单线程，多实例部署时需加锁或落库 |

---

## 9. 运行与联调

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/platform_server 8080
```

打开 <http://127.0.0.1:8080/>，左侧「学业分析与预警」卡片下列出全部 9 条路由，点按钮即可填入方法与路径，修改 Body 后发送即可看到真实响应。
