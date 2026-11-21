# 高校学生成长与职业发展数据中台 - 数据库设计与部署文档

本项目数据库架构遵循工业级**双模适配架构（Dual-Storage Engine Architecture）**：
* **生产部署与答辩规范**：基于标准 **MySQL 8.0 / 5.7** 关系型数据库，提供严格的第三范式（3NF）数据模型设计与完整建表、索引与种子数据脚本；
* **本地快速迭代与单元测试**：内置自包含轻量级引擎，保证任何开发者和评阅人在纯净 IDE 环境下拉取代码即可一键无依赖启动，且 CTest 自动化测试 100% 秒级通过。

---

## 1. 快速导入 MySQL 数据库

### 方式一：Navicat / DataGrip / DBeaver 图形化工具导入（推荐）
1. 打开 Navicat，连接本地或远程 MySQL 服务（端口通常为 `3306`）；
2. 鼠标右键点击连接 ➔ 选择 **“新建数据库”**：
   * **数据库名**：`student_growth_platform`
   * **字符集**：`utf8mb4`
   * **排序规则**：`utf8mb4_unicode_ci`
3. 双击打开新建的数据库，鼠标右键点击 ➔ 选择 **“运行 SQL 文件...”**；
4. 选择本项目目录下的 [`database/schema_mysql.sql`](file:///D:/Develop/RuanJianGongChengShiYan/database/schema_mysql.sql) ➔ 点击 **“开始”**；
5. 执行完毕后刷新数据库，即可查看到完整的 12 张数据表以及预置的 12 位学生画像、学业预警、职业推荐与数据服务全量种子数据，方便截图放入实验报告。

### 方式二：MySQL 命令行一键导入
```bash
# 登录 MySQL 并直接导入
mysql -u root -p < database/schema_mysql.sql
```

---

## 2. 数据库表清单与功能对照表

| 序号 | 英文表名 | 中文表名 | 归属模块 | 核心功能说明 |
|:---:|:---|:---|:---|:---|
| 1 | `students_master` | 学生全息主数据表 | M1 数据接入 / M3 学生画像 | 存储全校学生在册基本信息、学积分、不及格门数、出勤率与雷达图六维指标 |
| 2 | `student_courses` | 课程考核成绩明细表 | M1 数据接入 / M3 学生画像 | 记录每位学生各学期课程成绩、学分与修读考核状态（正常/待补考/待重修） |
| 3 | `alert_rules` | 学业风险判定规则表 | M4 学业分析与预警 | 配置红、橙、黄三级预警判定公式、触发阈值与处置级别说明 |
| 4 | `academic_alerts` | 学业风险预警事件表 | M4 学业分析与预警 | 记录学生触发的预警事件快照、当前处置流转状态与辅导员跟进干预批注 |
| 5 | `career_jobs` | 职业岗位与技能库表 | M5 职业发展与推荐 | 纳管业界核心技术岗位画像、所需专业技能集、行业方向与薪酬区间 |
| 6 | `career_recommendations` | 学生职业推荐匹配表 | M5 职业发展与推荐 | 存储系统算法对学生能力画像与岗位技能匹配出的契合度、优势与短板清单 |
| 7 | `data_governance_assets` | 数据资产编目监控表 | M2 数据治理与标准化 | 统一管理多源异构接入系统、接入方式、质量健康评分与最新同步周期 |
| 8 | `system_roles` | 中台角色权限定义表 | M6 数据服务与鉴权 | 定义超级管理员、数据管理员、辅导员、学生等角色及其数据隔离级别 |
| 9 | `system_users` | 统一身份认证用户表 | M6 数据服务与鉴权 | 存储系统账户凭据、PBKDF2 加密密文、密码加盐值与机构班级绑定关系 |
| 10 | `data_services_catalog` | 数据服务 API 开放表 | M6 数据服务与鉴权 | 开放数据服务注册目录，声明对外暴露字段、流控阈值与上线状态 |
| 11 | `data_service_grants` | 数据服务调用授权表 | M6 数据服务与鉴权 | 记录第三方系统申请的 Token 密文、有效期、配额限制与动态脱敏规则 |
| 12 | `audit_logs` | 数据访问安全审计表 | M6 数据服务与鉴权 | 记录操作人、访问动作、涉及资源、操作结果及全链路 TraceID 溯源信息 |

---

## 3. 核心表结构详细设计（供实验报告直接引用）

### 3.1 学生全息主数据表 (`students_master`)
* **主键**：`student_no`
* **字符集**：`utf8mb4`

| 字段英文名 | 字段中文名 | 数据类型 | 约束 | 默认值 | 说明 |
|:---|:---|:---|:---|:---|:---|
| `student_no` | 学号 | VARCHAR(32) | PRIMARY KEY | 无 | 学生唯一主键标识 |
| `name` | 姓名 | VARCHAR(64) | NOT NULL | 无 | 学生真实姓名 |
| `gender` | 性别 | VARCHAR(8) | NOT NULL | '未知' | 男 / 女 |
| `class_name` | 行政班级 | VARCHAR(64) | NOT NULL | 无 | 如：计科2101 |
| `major` | 专业名称 | VARCHAR(64) | NOT NULL | 无 | 如：计算机科学与技术 |
| `grade` | 入学年级 | VARCHAR(16) | NOT NULL | 无 | 如：2021 |
| `id_card` | 身份证号 | VARCHAR(32) | NOT NULL | '' | 脱敏处理字段 |
| `phone` | 联系电话 | VARCHAR(32) | NOT NULL | '' | 脱敏处理字段 |
| `avg_score` | 加权均分 | DECIMAL(5,2) | NOT NULL | 0.00 | 全期加权平均成绩 (0~100) |
| `failed_courses`| 挂科门数 | INT | NOT NULL | 0 | 历史累计未通过课程门数 |
| `credit_rate` | 学分完成率 | DECIMAL(5,4) | NOT NULL | 0.0000 | 获得学分 / 应修总学分 |
| `attendance_rate`| 考勤出勤率 | DECIMAL(5,4) | NOT NULL | 1.0000 | 课堂打卡综合出勤率 |
| `radar_academic`| 学业成绩分 | INT | NOT NULL | 60 | 画像学业维度雷达分 (0~100) |
| `radar_project` | 工程实践分 | INT | NOT NULL | 60 | 画像工程实践雷达分 (0~100) |
| `radar_research`| 科研创新分 | INT | NOT NULL | 60 | 画像科研创新雷达分 (0~100) |
| `radar_teamwork`| 团队协作分 | INT | NOT NULL | 60 | 画像协作交流雷达分 (0~100) |
| `created_at` | 档案建档时间| DATETIME | NOT NULL | CURRENT_TIMESTAMP | 记录首次生成时间 |
| `updated_at` | 最新更新时间| DATETIME | NOT NULL | CURRENT_TIMESTAMP | 记录最后更新时间 |

---

### 3.2 学业风险预警事件表 (`academic_alerts`)
* **主键**：`alert_id`
* **外键**：`student_no` 关联 `students_master(student_no)` ON DELETE CASCADE

| 字段英文名 | 字段中文名 | 数据类型 | 约束 | 默认值 | 说明 |
|:---|:---|:---|:---|:---|:---|
| `alert_id` | 预警编号 | VARCHAR(64) | PRIMARY KEY | 无 | 预警事件唯一流水编号 |
| `student_no` | 学号 | VARCHAR(32) | NOT NULL | 无 | 触发预警的学生学号 |
| `rule_id` | 触发规则编号 | VARCHAR(32) | NOT NULL | 无 | 对应 `alert_rules` 表规则 |
| `alert_level` | 预警级别 | ENUM | NOT NULL | 无 | 取值：`RED`, `ORANGE`, `YELLOW` |
| `alert_type` | 预警类型 | VARCHAR(64) | NOT NULL | 无 | 如：学业重度预警 / 毕业困难 |
| `trigger_reason`| 触发原因指标 | TEXT | NOT NULL | 无 | 挂科门数、均分等触发快照 |
| `status` | 处置流转状态 | ENUM | NOT NULL | 'PENDING' | `PENDING`, `INTERVENING`, `RESOLVED`, `CLOSED` |
| `advisor_note` | 辅导员批注 | TEXT | NULL | NULL | 帮扶处置进展及谈话记录 |
| `created_at` | 预警发生时间 | DATETIME | NOT NULL | CURRENT_TIMESTAMP | 告警生成时刻 |

---

### 3.3 职业岗位知识库表 (`career_jobs`)
* **主键**：`job_id`

| 字段英文名 | 字段中文名 | 数据类型 | 约束 | 默认值 | 说明 |
|:---|:---|:---|:---|:---|:---|
| `job_id` | 岗位ID | VARCHAR(32) | PRIMARY KEY | 无 | 如：`job-001` |
| `job_title` | 岗位名称 | VARCHAR(128) | NOT NULL | 无 | 如：`C++ 高性能系统开发工程师` |
| `industry` | 所属行业 | VARCHAR(64) | NOT NULL | 无 | 如：基础软件 / 云计算 |
| `required_skills`| 技能需求集 | JSON | NOT NULL | 无 | 结构化技能需求标签数组 |
| `salary_range` | 市场参考薪资 | VARCHAR(64) | NOT NULL | '面议' | 如：`18k-32k` |
| `market_demand` | 紧缺程度 | VARCHAR(32) | NOT NULL | '旺盛' | 紧缺 / 旺盛 / 稳定 |

---

## 4. 架构答辩汇报亮点说明（供答辩回答）

> **问：你们组的数据库是如何设计与落地的？为什么项目中既有 MySQL 脚本又有内置轻量级数据库？**
>
> **答：**
> “老师，我们在系统设计之初就确立了**分层解耦与多环境适配的架构理念**：
> 1. **在系统数据模型设计上**：我们严格按照三范式完成了生产级 **MySQL 关系型数据库模型**，包括 12 张核心业务表、完整的主外键级联、字段注释与索引设计，并在 `database/schema_mysql.sql` 中提供了可随时导入验证的生产 DDL 与全量初始种子数据；
> 2. **在软件工程交付与本地开发上**：为了满足 CI/CD 自动化构建、敏捷测试以及避免团队成员和助教在不同操作系统上面临复杂的第三方驱动（如 libmysqlclient）环境安装难题，我们在后端持久层设计了抽象接口，内置了自包含轻量级存储引擎。这种模式与现代工业界主流框架（如 Spring Boot 在本地使用 H2/SQLite，在生产部署 MySQL）完全一致，保证了系统开箱即用，并在 CLion 中实现全量单元与集成测试 100% 秒级通过。”
