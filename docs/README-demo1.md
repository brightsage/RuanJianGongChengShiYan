# 高校学生成长与职业发展数据中台（demo1）

> 软件工程实验 · 系统原型。架构沿用 `tlias-web-management` 的分层约定：
> **Controller → Service → Mapper(MyBatis XML) → MySQL**，统一返回 `Result` / `PageResult`，
> 前端是「Vue 3 全局构建 + Element Plus + ECharts」的单页控制台，不打前端构建工具、离线可跑。

---

## 1. 它解决什么问题

学生在校数据分散在教务、科研、实践、就业多个系统，口径不一致、画像缺失、服务缺失。本原型把 M1–M6 六个模块收成一条可演示的主干：

```
数据接入(M1) → 指标口径(M2) → 学生画像(M3) → 学业预警(M4) → 职业推荐(M5) → 数据服务出口(M6 基础)
```

一句话：**向下统一接入，向上提供「指标—标签—画像—预警—推荐」五层能力**。

## 2. 技术栈

| 层 | 选型 |
|---|---|
| 后端 | Spring Boot 4.0.8 · Java 17 · MyBatis 4.0.1 · MySQL 8.0 |
| 认证 | JWT（jjwt 0.9.1）+ HandlerInterceptor，前端请求头 `token` |
| 分页 | 手写 `LIMIT` + 独立 `count` 查询（不依赖分页插件，Boot 4 下更稳） |
| 前端 | Vue 3（全局构建）· Element Plus · ECharts · axios，依赖全部本地化到 `static/lib/` |
| 文档 | 本 README + 页面内「口径说明」提示 |

## 3. 目录结构

```text
demo1/
├─ pom.xml
├─ sql/init.sql                        ← 建表 + 演示数据（可重复执行）
├─ src/main/java/com/example/demo/
│  ├─ Demo1Application.java            ← @MapperScan + 事务
│  ├─ controller/                      ← 7 个 Controller
│  ├─ service/ + service/impl/         ← 业务与计算逻辑
│  ├─ mapper/                          ← MyBatis 接口
│  ├─ pojo/                            ← 实体 + 查询参数 + VO
│  ├─ utils/                           ← ScoreUtils（口径实现）、MaskUtils、JwtUtils
│  ├─ interceptor/LoginInterceptor.java
│  ├─ config/                          ← WebConfig、DataInitializer
│  └─ exception/GlobalExceptionHandler.java
└─ src/main/resources/
   ├─ application.properties
   ├─ mapper/*.xml                     ← 8 个映射文件（动态 SQL）
   └─ static/                          ← index.html + css/js + lib（离线依赖）
```

## 4. 快速开始

### 4.1 建库

数据库名 `ruanjianshiyan`，脚本只重建表、不删库：

```powershell
& "D:\Develop\mysql-8.0.34-winx64\mysql-8.0.34-winx64\bin\mysql.exe" -uroot -p1234 --default-character-set=utf8mb4 -e "source D:/Develop/demo1/sql/init.sql"
```

> 数据库连接在 `src/main/resources/application.properties`，默认 `root/1234`，按本机改。
> SQL 只灌**原始数据**（学院、学生、指标口径、预警规则、岗位、数据源），
> 画像快照 / 预警记录 / 推荐结果由应用首次启动时算出来，避免「脚本数字」和「代码算法」两套口径。

### 4.2 启动

IDE 里直接 Run `Demo1Application`；或命令行：

```powershell
mvn -o -DskipTests package
java -jar target\demo1-0.0.1-SNAPSHOT.jar
```

浏览器打开 <http://localhost:8080>。

### 4.3 演示账号

| 账号 | 密码 | 角色 | 能看到的差异 |
|---|---|---|---|
| admin | admin123 | 管理员 | 手机号、身份证号显示原值 |
| teacher | teacher123 | 辅导员 | 敏感字段自动脱敏（`138****0001`） |
| student | student123 | 学生 | 同上，且登录后默认看学生视角 |

首次启动会自动计算 30 名学生的画像、按 5 条规则生成预警、为每人生成推荐，控制台日志里有 `[初始化]` 记录。

## 5. 页面与功能

| 页面 | 主要能力 | 对应模块 |
|---|---|---|
| 数据总览 | 5 个指标卡 + 学院分布 / 能力雷达 / 预警等级 / 就业意向 / 年级分布 5 张图 | M3、M4 |
| 学生画像 | 条件分页、画像抽屉（标签、雷达、指标明细、预警、推荐）、单人/全量重算、**新增 / 编辑 / 删除学生（仅管理员）** | M3 |
| 指标口径 | 8 项指标的增删改、启停、权重合计校验；口径变更后重算即生效 | M2 |
| 学业预警 | 多条件查询、按规则生成、认领/处置、规则启停 | M4 |
| 职业推荐 | 岗位库 CRUD（含能力要求编辑）、按学生生成推荐（匹配度 + 理由 + 差距） | M5 |
| 数据接入 | 数据源登记、增量同步、导入批次（成功/部分成功） | M1 |

### 界面预览（`docs/screenshots/`）

| 页面 | 截图 |
|---|---|
| 数据总览 | `01-dashboard.png` |
| 学生列表 | `02-students.png` |
| 学生画像抽屉 | `03-profile.png` |
| 指标口径 | `04-metrics.png` |
| 学业预警 | `05-alerts.png` |
| 推荐演示 | `07-recommend.png` |
| 数据接入 | `08-data.png` |
| 新增学生 | `09-student-new.png` |

## 6. 数据模型（14 张表）

| 分组 | 表 | 说明 |
|---|---|---|
| 组织人员 | `college`、`major`、`student`、`sys_user` | 学生主数据以学号唯一 |
| 治理 | `metric_def`、`student_metric`、`student_profile` | 口径、指标值、画像快照三层分离 |
| 分析预警 | `alert_rule`、`alert_record` | 规则触发 + 处置闭环 |
| 职业 | `job`、`job_requirement`、`recommend_record` | 能力要求与指标同源 |
| 接入 | `data_source`、`import_task` | 数据源与导入批次 |

关键关系：`student 1—N student_metric N—1 metric_def`；`student 1—1 student_profile`；
`job 1—N job_requirement`；`alert_rule 1—N alert_record N—1 student`。

## 7. 核心设计

**① 口径可配（G2）**
归一化与综合分只在 `ScoreUtils` 实现一次：正向 `value/max×100`，逆向 `(max-value)/max×100`，
综合分 `Σ(权重×归一化)/Σ权重`。改权重/满分/方向只改 `metric_def`，代码不动。
停用某指标后，综合分自动按剩余权重重新归一。

**② 指标 → 标签 → 画像（G3）**
标签按阈值从指标值派生（如「学业标杆」需要绩点 ≥ 4.0 且无不及格），每个标签带 `reason`，
画像抽屉里悬浮可见依据；预警记录里保存了触发当时的指标值，事后可追溯。

**③ 预警闭环（M4）**
规则 = 指标 + 比较符 + 阈值 + 等级，可启停；生成时按「学生 × 规则」幂等，已关闭的不再重复打扰；
处置状态只有「处理中 / 已关闭」，关闭必须填写干预措施（前后端都校验）。

**④ 可解释推荐（M5）**
匹配度 = 60% × 权重达标率 + 40% × 接近度，接近度 = `min(学生值/门槛值,1)` 的加权平均。
结果同时给出「满足」与「差距」明细，避免只给一个分数。
低于 50 分不推荐，最多推荐 4 个岗位，重复生成覆盖旧结果而不是叠加。

**⑤ 安全与脱敏（G5）**
JWT 拦截器保护除登录页与静态资源外的所有接口；401 时前端自动回登录页。
非管理员角色的手机号、身份证号在服务层脱敏，管理员可见原值。

**⑥ 可重复执行**
学生画像重算用 `upsert`（同一学生同一周期覆盖写）；预警按规则幂等；推荐先删后插。
所以「全量重算」点多少次，结果都一致。

## 8. 接口清单

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/login` | 登录，返回 JWT |
| GET | `/dashboard` | 看板统计 |
| GET | `/colleges`、`/majors` | 下拉基础数据 |
| GET | `/students?page&pageSize&keyword&collegeId&grade` | 学生分页 |
| GET | `/students/{id}` | 学生画像 |
| POST | `/students/{id}/recalc` | 单人重算（画像+预警+推荐） |
| POST | `/students/recalc-all` | 全量重算 |
| POST | `/students` | 新增学生（可带 `metrics` 初始指标，仅管理员） |
| PUT | `/students` | 修改学生基本信息（仅管理员） |
| DELETE | `/students?id=` | 删除学生及其派生数据（仅管理员） |
| GET | `/metrics`，POST/PUT/DELETE `/metrics` | 指标口径维护 |
| GET | `/alerts?page&pageSize&level&status&keyword` | 预警分页 |
| PUT | `/alerts/handle` | 处置预警 |
| POST | `/alerts/generate` | 按规则生成 |
| GET | `/alerts/rules`，PUT `/alerts/rules/{id}/enabled` | 规则查询与启停 |
| GET | `/jobs?page&pageSize&keyword`，GET `/jobs/{id}` | 岗位查询 |
| POST/PUT | `/jobs`，DELETE `/jobs?id=` | 岗位维护（含能力要求） |
| GET | `/recommends?studentId=` | 某学生的推荐 |
| POST | `/recommends/generate?studentId=` | 生成推荐 |
| GET | `/datasources`，POST `/datasources/{id}/sync` | 数据源与同步 |
| GET | `/import-tasks?page&pageSize` | 导入批次 |

统一返回：`{ "code": 1, "msg": "success", "data": ... }`，失败 `code=0` 且 `msg` 为可读原因。

## 9. 三分钟演示脚本

1. `student/student123` 登录 → 学生画像页，手机号已脱敏，看到自己的标签与雷达图；
2. 打开「指标口径」，把 `PRACTICE 实践与实习` 权重从 15 改成 25、`GPA` 从 20 改成 10，保存；
3. 回「学生画像」点「全量重算画像」→ 综合分与雷达图变化，说明**改口径不用改代码**；
4. 切到「学业预警」，筛选「高」等级，处置一条 → 关闭时必须填干预措施；
5. 切到「职业推荐 → 推荐演示」，换一个绩点较低的学生生成推荐 → 匹配度下降、差距项变多；
6. 切到「数据接入」点一次「增量同步」→ 新增一个导入批次；
7. 回「学生画像」点「+ 新增学生」（仅管理员可见）→ 填学号/姓名/学院/专业，指标值可改 →
   保存后列表里立刻出现综合分与标签，说明**建档即算画像**。

## 10. 边界与后续

- 演示用：密码明文存储、JWT 密钥写在配置里、同步是模拟行为，生产环境必须替换；
- 未做：真实文件解析与字段映射、指标口径版本灰度与回溯、数据服务 API 的授权/限流（M6 完整版）、审计日志表；
- 推荐尚未引入协同过滤或机器学习模型，本期是「规则 + 权重」的可解释算法；
- 前端未做路由分包，页面前端都在一个 `app.js` 里，适合教学阅读，不适合长期演进。
