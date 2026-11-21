-- ============================================================================
-- 高校学生成长与职业发展数据中台 - 生产级关系型数据库模型 (MySQL 8.0/5.7)
-- Database: student_growth_platform
-- Character Set: utf8mb4 / Collation: utf8mb4_unicode_ci
-- ============================================================================

CREATE DATABASE IF NOT EXISTS `student_growth_platform`
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE `student_growth_platform`;

SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------------------------------------------------------
-- 1. [M1 & M3] 学生全息主数据表 (students_master)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `students_master`;
CREATE TABLE `students_master` (
  `student_no` VARCHAR(32) NOT NULL COMMENT '学号 (主键)',
  `name` VARCHAR(64) NOT NULL COMMENT '学生姓名',
  `gender` VARCHAR(8) NOT NULL DEFAULT '未知' COMMENT '性别 (男/女)',
  `class_name` VARCHAR(64) NOT NULL COMMENT '行政班级 (如: 计科2101)',
  `major` VARCHAR(64) NOT NULL COMMENT '所属专业 (如: 计算机科学与技术)',
  `grade` VARCHAR(16) NOT NULL COMMENT '入学年级 (如: 2021)',
  `id_card` VARCHAR(32) NOT NULL DEFAULT '' COMMENT '身份证号 (脱敏)',
  `phone` VARCHAR(32) NOT NULL DEFAULT '' COMMENT '联系电话 (脱敏)',
  `avg_score` DECIMAL(5,2) NOT NULL DEFAULT 0.00 COMMENT '全期加权平均成绩',
  `failed_courses` INT NOT NULL DEFAULT 0 COMMENT '历史累计不及格门数',
  `credit_rate` DECIMAL(5,4) NOT NULL DEFAULT 0.0000 COMMENT '学分获得完成率 (0.0~1.0)',
  `attendance_rate` DECIMAL(5,4) NOT NULL DEFAULT 1.0000 COMMENT '综合考勤出勤率 (0.0~1.0)',
  `advisor_name` VARCHAR(64) NOT NULL DEFAULT '' COMMENT '责任辅导员/班主任姓名',
  `radar_academic` INT NOT NULL DEFAULT 60 COMMENT '画像雷达: 学业成绩维度 (0~100)',
  `radar_project` INT NOT NULL DEFAULT 60 COMMENT '画像雷达: 工程实践维度 (0~100)',
  `radar_research` INT NOT NULL DEFAULT 60 COMMENT '画像雷达: 科研创新维度 (0~100)',
  `radar_comprehensive` INT NOT NULL DEFAULT 60 COMMENT '画像雷达: 综合素质维度 (0~100)',
  `radar_discipline` INT NOT NULL DEFAULT 60 COMMENT '画像雷达: 纪律考勤维度 (0~100)',
  `radar_teamwork` INT NOT NULL DEFAULT 60 COMMENT '画像雷达: 团队协作维度 (0~100)',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '档案建立时间',
  `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后更新时间',
  PRIMARY KEY (`student_no`),
  KEY `idx_class_major` (`class_name`, `major`),
  KEY `idx_grade` (`grade`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='学生全息主数据表 (M1数据接入/M3学生画像)';

-- ----------------------------------------------------------------------------
-- 2. [M1 & M3] 学生课程成绩明细表 (student_courses)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `student_courses`;
CREATE TABLE `student_courses` (
  `id` BIGINT AUTO_INCREMENT COMMENT '成绩明细记录ID',
  `student_no` VARCHAR(32) NOT NULL COMMENT '学号',
  `course_code` VARCHAR(32) NOT NULL COMMENT '课程编码 (如: CS101)',
  `course_name` VARCHAR(128) NOT NULL COMMENT '课程名称',
  `credits` DECIMAL(3,1) NOT NULL DEFAULT 1.0 COMMENT '课程学分',
  `term` VARCHAR(32) NOT NULL COMMENT '开课学期 (如: 2024-2025-1)',
  `score` DECIMAL(5,2) NOT NULL COMMENT '期末考核得分',
  `exam_status` VARCHAR(32) NOT NULL DEFAULT '正常' COMMENT '修读状态 (正常/待补考/待重修)',
  PRIMARY KEY (`id`),
  KEY `idx_student_term` (`student_no`, `term`),
  CONSTRAINT `fk_course_student` FOREIGN KEY (`student_no`) REFERENCES `students_master` (`student_no`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='学生课程考核成绩明细表';

-- ----------------------------------------------------------------------------
-- 3. [M4] 学业分析与风险预警规则表 (alert_rules)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `alert_rules`;
CREATE TABLE `alert_rules` (
  `rule_id` VARCHAR(32) NOT NULL COMMENT '规则唯一编码',
  `rule_name` VARCHAR(128) NOT NULL COMMENT '规则名称 (如: 学业重度挂科预警)',
  `alert_level` ENUM('RED', 'ORANGE', 'YELLOW') NOT NULL COMMENT '预警级别 (红/橙/黄)',
  `condition_formula` TEXT NOT NULL COMMENT '判定指标公式',
  `description` VARCHAR(255) NOT NULL DEFAULT '' COMMENT '规则触发与帮扶处置要求',
  `is_active` TINYINT(1) NOT NULL DEFAULT 1 COMMENT '是否启用 (1=启用, 0=停用)',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  PRIMARY KEY (`rule_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='学业风险判定规则配置表 (M4学业分析)';

-- ----------------------------------------------------------------------------
-- 4. [M4] 学业风险预警事件记录表 (academic_alerts)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `academic_alerts`;
CREATE TABLE `academic_alerts` (
  `alert_id` VARCHAR(64) NOT NULL COMMENT '预警事件唯一编码',
  `student_no` VARCHAR(32) NOT NULL COMMENT '预警学生学号',
  `rule_id` VARCHAR(32) NOT NULL COMMENT '触发的规则编号',
  `alert_level` ENUM('RED', 'ORANGE', 'YELLOW') NOT NULL COMMENT '预警级别',
  `alert_type` VARCHAR(64) NOT NULL COMMENT '预警类型 (学业成绩/考勤异常/毕业学分不足)',
  `trigger_reason` TEXT NOT NULL COMMENT '触发原因详细指标快照',
  `status` ENUM('PENDING', 'INTERVENING', 'RESOLVED', 'CLOSED') NOT NULL DEFAULT 'PENDING' COMMENT '处置状态',
  `advisor_note` TEXT COMMENT '辅导员介入处理说明',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '产生时间',
  `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`alert_id`),
  KEY `idx_alert_student` (`student_no`),
  KEY `idx_alert_status` (`status`, `alert_level`),
  CONSTRAINT `fk_alert_student` FOREIGN KEY (`student_no`) REFERENCES `students_master` (`student_no`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='学业风险预警与帮扶事件表 (M4学业预警)';

-- ----------------------------------------------------------------------------
-- 5. [M5] 职业岗位画像与技能库表 (career_jobs)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `career_jobs`;
CREATE TABLE `career_jobs` (
  `job_id` VARCHAR(32) NOT NULL COMMENT '岗位唯一ID (如: job-001)',
  `job_title` VARCHAR(128) NOT NULL COMMENT '岗位名称 (如: C++ 高性能系统开发工程师)',
  `industry` VARCHAR(64) NOT NULL COMMENT '行业细分领域 (如: 基础软件/云计算)',
  `required_skills` JSON NOT NULL COMMENT '核心技能需求集合 (JSON 数组格式)',
  `salary_range` VARCHAR(64) NOT NULL DEFAULT '面议' COMMENT '市场参考月薪 (如: 18k-30k)',
  `market_demand` VARCHAR(32) NOT NULL DEFAULT '旺盛' COMMENT '招聘紧缺程度 (高需求/旺盛/稳定)',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '录入时间',
  PRIMARY KEY (`job_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='职业岗位与技能画像知识库表 (M5职业推荐)';

-- ----------------------------------------------------------------------------
-- 6. [M5] 学生职业智能推荐匹配表 (career_recommendations)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `career_recommendations`;
CREATE TABLE `career_recommendations` (
  `rec_id` VARCHAR(64) NOT NULL COMMENT '推荐流水唯一编号',
  `student_no` VARCHAR(32) NOT NULL COMMENT '匹配推荐学生学号',
  `job_id` VARCHAR(32) NOT NULL COMMENT '推荐目标岗位ID',
  `match_score` DECIMAL(5,2) NOT NULL COMMENT '契合度综合评分 (0~100)',
  `matched_skills` JSON NOT NULL COMMENT '已具备的核心能力优势列表',
  `skill_gaps` JSON NOT NULL COMMENT '尚存差距的能力短板与修读建议',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '计算生成时间',
  PRIMARY KEY (`rec_id`),
  KEY `idx_rec_student` (`student_no`),
  CONSTRAINT `fk_rec_student` FOREIGN KEY (`student_no`) REFERENCES `students_master` (`student_no`) ON DELETE CASCADE,
  CONSTRAINT `fk_rec_job` FOREIGN KEY (`job_id`) REFERENCES `career_jobs` (`job_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='学生职业发展推荐结果表';

-- ----------------------------------------------------------------------------
-- 7. [M2] 数据资产编目与源系统接入监控表 (data_governance_assets)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `data_governance_assets`;
CREATE TABLE `data_governance_assets` (
  `asset_id` VARCHAR(32) NOT NULL COMMENT '数据资产编号 (如: DA-001)',
  `asset_name` VARCHAR(128) NOT NULL COMMENT '数据资产名称 (如: 全校教务学籍主库)',
  `source_system` VARCHAR(64) NOT NULL COMMENT '来源业务系统 (如: 正方教务系统)',
  `sync_mode` VARCHAR(32) NOT NULL DEFAULT '实时API' COMMENT '接入模式 (实时API/CDC流式/定时批处理)',
  `quality_score` DECIMAL(4,1) NOT NULL DEFAULT 95.0 COMMENT '数据质量综合评分 (0~100)',
  `total_records` BIGINT NOT NULL DEFAULT 0 COMMENT '资产纳管记录总条数',
  `status` VARCHAR(16) NOT NULL DEFAULT '正常' COMMENT '运行状态 (正常/告警/异常)',
  `last_sync_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '最新一次数据同步时间',
  PRIMARY KEY (`asset_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='数据治理主数据与数据资产表 (M2数据治理)';

-- ----------------------------------------------------------------------------
-- 8. [M6] 中台角色与统一权限管理表 (system_roles)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `system_roles`;
CREATE TABLE `system_roles` (
  `code` VARCHAR(32) NOT NULL COMMENT '角色标识符 (如: system_admin)',
  `name` VARCHAR(64) NOT NULL COMMENT '角色中文名 (如: 系统管理员)',
  `scope_type` VARCHAR(16) NOT NULL DEFAULT 'all' COMMENT '数据可见范围 (all/college/class/self)',
  `permissions` JSON NOT NULL COMMENT '授权功能权限码集合 (JSON 数组格式)',
  PRIMARY KEY (`code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='中台角色定义与权限范围表 (M6权限管理)';

-- ----------------------------------------------------------------------------
-- 9. [M6] 中台统一用户凭据表 (system_users)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `system_users`;
CREATE TABLE `system_users` (
  `id` VARCHAR(64) NOT NULL COMMENT '用户唯一编号',
  `username` VARCHAR(64) NOT NULL COMMENT '登录账号',
  `password_hash` VARCHAR(128) NOT NULL COMMENT 'PBKDF2 安全哈希密文',
  `salt` VARCHAR(64) NOT NULL COMMENT '密码派生盐值',
  `role_code` VARCHAR(32) NOT NULL COMMENT '归属角色代码',
  `org_id` VARCHAR(64) NOT NULL DEFAULT '' COMMENT '所属机构/学院代码',
  `class_id` VARCHAR(64) NOT NULL DEFAULT '' COMMENT '管理班级代码 (辅导员专有)',
  `student_no` VARCHAR(32) NOT NULL DEFAULT '' COMMENT '绑定学号 (学生角色专有)',
  `is_active` TINYINT(1) NOT NULL DEFAULT 1 COMMENT '账号状态 (1=正常, 0=禁用)',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_username` (`username`),
  CONSTRAINT `fk_user_role` FOREIGN KEY (`role_code`) REFERENCES `system_roles` (`code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='统一身份认证与用户凭据表';

-- ----------------------------------------------------------------------------
-- 10. [M6] 数据服务 API 开放目录表 (data_services_catalog)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `data_services_catalog`;
CREATE TABLE `data_services_catalog` (
  `service_id` VARCHAR(64) NOT NULL COMMENT '服务唯一ID',
  `service_name` VARCHAR(128) NOT NULL COMMENT 'API 服务名称',
  `provider_id` VARCHAR(64) NOT NULL COMMENT '发布提供者机构',
  `version` VARCHAR(16) NOT NULL DEFAULT 'v1' COMMENT 'API 语义化版本号',
  `fields_json` JSON NOT NULL COMMENT '该服务对外开放的数据字段列表',
  `scope_type` VARCHAR(32) NOT NULL DEFAULT 'all' COMMENT '数据隔离级别',
  `rate_per_minute` INT NOT NULL DEFAULT 60 COMMENT '每分钟限流调用频次',
  `is_active` TINYINT(1) NOT NULL DEFAULT 1 COMMENT '服务发布状态 (1=上线, 0=下线)',
  PRIMARY KEY (`service_id`),
  UNIQUE KEY `uk_service_ver` (`service_name`, `version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='数据服务开放目录与限流表 (M6数据服务)';

-- ----------------------------------------------------------------------------
-- 11. [M6] 数据授权令牌与配额契约表 (data_service_grants)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `data_service_grants`;
CREATE TABLE `data_service_grants` (
  `grant_id` VARCHAR(64) NOT NULL COMMENT '授权凭证流水号',
  `owner_user_id` VARCHAR(64) NOT NULL COMMENT '持有者用户编号',
  `service_id` VARCHAR(64) NOT NULL COMMENT '授权调用的API服务ID',
  `token_hash` VARCHAR(128) NOT NULL COMMENT 'API 调用密钥SHA-256摘要',
  `scope` VARCHAR(128) NOT NULL COMMENT '实际授权的数据访问范围',
  `mask_level` VARCHAR(32) NOT NULL DEFAULT 'partial' COMMENT '动态脱敏规则级别 (redact/partial/none)',
  `quota_total` INT NOT NULL COMMENT '分配的总调用配额',
  `quota_used` INT NOT NULL DEFAULT 0 COMMENT '已消耗调用量',
  `rate_per_minute` INT NOT NULL DEFAULT 60 COMMENT '每分钟流控阈值',
  `expires_at` DATETIME NOT NULL COMMENT '凭证过期截止时间',
  `is_revoked` TINYINT(1) NOT NULL DEFAULT 0 COMMENT '是否已吊销 (1=已吊销, 0=有效)',
  PRIMARY KEY (`grant_id`),
  KEY `idx_token_hash` (`token_hash`),
  CONSTRAINT `fk_grant_user` FOREIGN KEY (`owner_user_id`) REFERENCES `system_users` (`id`),
  CONSTRAINT `fk_grant_svc` FOREIGN KEY (`service_id`) REFERENCES `data_services_catalog` (`service_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='数据开放调用授权与脱敏凭证表';

-- ----------------------------------------------------------------------------
-- 12. [M6] 数据访问合规审计日志表 (audit_logs)
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `audit_logs`;
CREATE TABLE `audit_logs` (
  `log_id` BIGINT AUTO_INCREMENT COMMENT '审计自增主键',
  `actor_id` VARCHAR(64) NOT NULL COMMENT '操作人/客户端标识',
  `action` VARCHAR(64) NOT NULL COMMENT '操作动作 (如: QUERY_STUDENT_PROFILE)',
  `resource_type` VARCHAR(64) NOT NULL COMMENT '涉及资源大类',
  `resource_id` VARCHAR(64) NOT NULL COMMENT '目标资源标识',
  `outcome` VARCHAR(16) NOT NULL COMMENT '操作结果 (SUCCESS / DENIED / FAILED)',
  `reason` VARCHAR(255) NOT NULL DEFAULT '' COMMENT '判定原因或错误日志',
  `request_id` VARCHAR(64) NOT NULL COMMENT '全链路追踪跟踪ID',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '审计发生时间',
  PRIMARY KEY (`log_id`),
  KEY `idx_audit_actor` (`actor_id`, `created_at`),
  KEY `idx_audit_time` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='全链路数据访问安全审计日志表';

SET FOREIGN_KEY_CHECKS = 1;

-- ============================================================================
-- 完整预置初始全量种子数据 (Seed Master Data)
-- ============================================================================

-- 1. 预置系统角色
INSERT INTO `system_roles` (`code`, `name`, `scope_type`, `permissions`) VALUES
('system_admin', '系统管理员', 'all', '["users.manage", "roles.manage", "masking.manage", "audit.read"]'),
('data_admin', '数据治理管理员', 'all', '["services.publish", "grants.approve", "services.read", "governance.manage"]'),
('counselor', '辅导员教师', 'class', '["students.read", "alerts.read", "alerts.handle", "recommendations.read"]'),
('career_center', '就业指导中心', 'all', '["career.jobs.read", "career.jobs.write", "career.jobs.publish", "career.strategy.write"]'),
('student', '在校学生', 'self', '["profile.self.read", "career.recommendations.read"]')
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`);

-- 2. 预置超级管理员账号 (初始账号: admin, 初始密码: Admin12345678)
INSERT INTO `system_users` (`id`, `username`, `password_hash`, `salt`, `role_code`, `org_id`, `class_id`, `student_no`, `is_active`) VALUES
('u_admin_root', 'admin', '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', 'c1a2b3d4e5f60718', 'system_admin', 'ROOT', '', '', 1),
('u_counselor_li', 'counselor_li', '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', 'c1a2b3d4e5f60718', 'counselor', 'CS_DEPT', '计科2101', '', 1)
ON DUPLICATE KEY UPDATE `username`=VALUES(`username`);

-- 3. 预置全校 12 位学生全息成长主数据 (覆盖优秀生、学业预警生、实践特长生等不同类型)
INSERT INTO `students_master` 
(`student_no`, `name`, `gender`, `class_name`, `major`, `grade`, `avg_score`, `failed_courses`, `credit_rate`, `attendance_rate`, `advisor_name`, `radar_academic`, `radar_project`, `radar_research`, `radar_comprehensive`, `radar_discipline`, `radar_teamwork`) 
VALUES
('2021001', '张伟', '男', '计科2101', '计算机科学与技术', '2021', 58.40, 3, 0.5200, 0.7200, '李老师', 58, 78, 45, 62, 65, 70),
('2021002', '李娜', '女', '计科2101', '计算机科学与技术', '2021', 92.50, 0, 1.0000, 0.9800, '李老师', 95, 88, 92, 94, 98, 90),
('2021003', '王强', '男', '计科2101', '计算机科学与技术', '2021', 48.20, 5, 0.3800, 0.6100, '李老师', 45, 60, 30, 50, 55, 58),
('2021004', '赵敏', '女', '计科2102', '计算机科学与技术', '2021', 86.70, 0, 0.9800, 0.9500, '李老师', 88, 92, 80, 89, 94, 91),
('2021005', '孙浩', '男', '计科2102', '计算机科学与技术', '2021', 61.30, 2, 0.6500, 0.8100, '李老师', 60, 82, 55, 68, 72, 75),
('2021006', '周婷', '女', '软件2101', '软件工程', '2021', 94.80, 0, 1.0000, 0.9900, '王老师', 98, 95, 96, 97, 98, 96),
('2021007', '吴磊', '男', '软件2101', '软件工程', '2021', 52.10, 4, 0.4400, 0.6800, '王老师', 50, 70, 40, 55, 60, 62),
('2021008', '郑爽', '女', '软件2102', '软件工程', '2021', 79.40, 0, 0.9200, 0.9100, '王老师', 80, 85, 75, 82, 88, 84),
('2021009', '陈杰', '男', '网络2101', '网络工程', '2021', 73.60, 1, 0.8500, 0.8900, '张老师', 72, 84, 68, 76, 85, 80),
('2021010', '杨洋', '女', '网络2101', '网络工程', '2021', 88.90, 0, 0.9600, 0.9600, '张老师', 90, 86, 88, 91, 95, 88),
('2021011', '刘博', '男', '大数据2101', '数据科学与大数据技术', '2021', 44.50, 6, 0.3100, 0.5500, '陈老师', 42, 55, 35, 48, 52, 50),
('2021012', '黄悦', '女', '大数据2101', '数据科学与大数据技术', '2021', 91.20, 0, 1.0000, 0.9700, '陈老师', 93, 89, 90, 92, 96, 92)
ON DUPLICATE KEY UPDATE `avg_score`=VALUES(`avg_score`);

-- 4. 预置学业预警判定规则
INSERT INTO `alert_rules` (`rule_id`, `rule_name`, `alert_level`, `condition_formula`, `description`) VALUES
('RULE_ACADEMIC_RED', '挂科 >= 4 门或均分 < 55分', 'RED', 'failed_courses >= 4 || avg_score < 55.0', '触发红色重度预警，需辅导员立即约谈并发出学业警示函'),
('RULE_ACADEMIC_ORANGE', '挂科 2~3 门或均分 < 60分', 'ORANGE', 'failed_courses >= 2 && avg_score < 60.0', '触发橙色中度预警，安排学习帮扶对子并通报班主任'),
('RULE_ATTENDANCE_YELLOW', '学期打卡出勤率 < 75%', 'YELLOW', 'attendance_rate < 0.75', '触发黄色考勤预警，督促课堂出勤打卡')
ON DUPLICATE KEY UPDATE `rule_name`=VALUES(`rule_name`);

-- 5. 预置学业预警事件记录
INSERT INTO `academic_alerts` (`alert_id`, `student_no`, `rule_id`, `alert_level`, `alert_type`, `trigger_reason`, `status`, `advisor_note`) VALUES
('ALT-202511-001', '2021003', 'RULE_ACADEMIC_RED', 'RED', '学业重度预警', '累计不及格课程达到5门，本期加权平均成绩仅48.2分', 'INTERVENING', '已于11月12日约谈王强同学，已联系家长寄送学业预警告知书，并安排优秀党员进行一对一高数与组成原理补习。'),
('ALT-202511-002', '2021011', 'RULE_ACADEMIC_RED', 'RED', '退学预警边缘', '累计不及格课程多达6门，学分完成率仅31%，出勤率仅55%', 'PENDING', '待安排教学院长与辅导员联合诫勉谈话。'),
('ALT-202511-003', '2021001', 'RULE_ACADEMIC_ORANGE', 'ORANGE', '学业中度预警', '当前学期不及格课程3门，均分58.4分，出勤率72%', 'INTERVENING', '张伟同学工程实践能力突出（得分78），但理论课投入时间不足。已指导制定重修时间规划表。'),
('ALT-202511-004', '2021007', 'RULE_ACADEMIC_RED', 'RED', '学业重度预警', '累计挂科4门，均分52.1分', 'PENDING', '待跟进期中补考复习进度。')
ON DUPLICATE KEY UPDATE `status`=VALUES(`status`);

-- 6. 预置职业岗位知识库
INSERT INTO `career_jobs` (`job_id`, `job_title`, `industry`, `required_skills`, `salary_range`, `market_demand`) VALUES
('job-001', 'C++ 高性能系统/后端研发工程师', '基础软件/分布式架构', '["C++17", "Linux高性能编程", "多线程高并发", "Socket网络编程", "数据结构与算法", "SQLite/MySQL"]', '18k-32k', '紧缺'),
('job-002', '数据治理与大数据架构工程师', '数智金融/数据中台', '["SQL复杂查询", "数据模型设计", "ETL数据清洗", "分布式存储", "数据质量监控", "Python"]', '16k-28k', '旺盛'),
('job-003', '云计算平台研发工程师', '公有云/网络安全', '["云原生容器化", "微服务架构", "API安全网关", "RESTful协议", "Linux内核", "Go/C++"]', '20k-35k', '紧缺'),
('job-004', '全栈与现代 Web 交互工程师', '企业协同/智慧校园', '["现代前端交互", "REST API设计", "CSS3响应式", "跨平台前后端联调", "JavaScript/TypeScript"]', '14k-24k', '稳定')
ON DUPLICATE KEY UPDATE `job_title`=VALUES(`job_title`);

-- 7. 预置学生职业匹配推荐结果
INSERT INTO `career_recommendations` (`rec_id`, `student_no`, `job_id`, `match_score`, `matched_skills`, `skill_gaps`) VALUES
('REC-2021002-001', '2021002', 'job-001', 94.50, '["精通 C++ 标准", "数据结构优秀(96分)", "操作系统优秀(94分)", "算法设计底蕴扎实"]', '["建议进一步深入内核级 epoll 优化实践"]'),
('REC-2021006-001', '2021006', 'job-003', 96.80, '["软件体系结构卓越", "微服务接口设计优秀", "多语言工程能力扎实", "团队管理与协作拔尖"]', '["可继续冲刺开源社区顶级项目贡献"]'),
('REC-2021001-001', '2021001', 'job-004', 78.00, '["具备良好的前端交互感知", "具有一定的工程落地意愿"]', '["必须立即优先通过基础课程重修，补齐数据结构与计算机原理理论短板"]')
ON DUPLICATE KEY UPDATE `match_score`=VALUES(`match_score`);

-- 8. 预置数据治理监控资产
INSERT INTO `data_governance_assets` (`asset_id`, `asset_name`, `source_system`, `sync_mode`, `quality_score`, `total_records`, `status`) VALUES
('DA-001', '全校本科生学籍主数据', '教务一体化系统', 'CDC增量同步', 98.6, 28450, '正常'),
('DA-002', '学期期末考试成绩库', '教务考务管理系统', '定时批量拉取', 94.2, 142300, '正常'),
('DA-003', '智慧校园课堂与门禁考勤流', '物联网边缘网关', 'Kafka实时消费', 91.5, 982100, '正常'),
('DA-004', '应届生毕业生就业去向统计库', '就创业服务大厅', 'REST API接入', 99.1, 4520, '正常')
ON DUPLICATE KEY UPDATE `asset_name`=VALUES(`asset_name`);

-- 9. 预置开放数据服务 API
INSERT INTO `data_services_catalog` (`service_id`, `service_name`, `provider_id`, `version`, `fields_json`, `scope_type`, `rate_per_minute`, `is_active`) VALUES
('svc_student_profile', '学生综合全息画像服务', '教务与学工处', 'v1', '["student_no", "name", "avg_score", "radar_academic", "radar_project"]', 'all', 120, 1),
('svc_alerts_notify', '学业风险预警事件推送服务', '学业指导中心', 'v1', '["alert_id", "student_no", "alert_level", "trigger_reason", "status"]', 'college', 60, 1),
('svc_career_matching', '学生职业契合度智能计算服务', '就业指导中心', 'v1', '["student_no", "job_id", "match_score", "matched_skills", "skill_gaps"]', 'all', 90, 1)
ON DUPLICATE KEY UPDATE `service_name`=VALUES(`service_name`);
