# M4 学业分析与预警

负责人：刘家庚（feature/LiuJiaGeng）
路由前缀：`/api/modules/academic-alerts/`

对应需求：实验一需求分析 §2.3.5，用例 UC-AA-01 ～ UC-AA-07。

## 接口清单

| 用例 | 方法 | 路径 | 说明 |
| --- | --- | --- | --- |
| UC-AA-01 查看学业看板 | GET | `/dashboard` | 学生数、平均分、挂科人数、预警数与超时数；`?class=` / `?major=` 过滤 |
| UC-AA-02 群体分析 | GET | `/dashboard?group=class\|major\|grade` | 按班级 / 专业 / 年级分组统计 |
| UC-AA-03 配置预警规则 | GET | `/rules` | 查看已配置规则 |
| | POST | `/rules` | 新增规则，body：`metric`、`operator`、`threshold`、`level`、`name`、`notify` |
| UC-AA-04 生成预警名单 | POST | `/alerts/generate` | 按启用规则计算并生成分级预警（幂等） |
| | GET | `/alerts` | 预警列表，支持 `?status=`、`?level=`、`?class=` |
| UC-AA-05 处置预警 | POST | `/alerts/:alert_id/claim` | 认领，body：`assignee` |
| | POST | `/alerts/:alert_id/handle` | 处置，body：`action`（follow/close/not_applicable）、`measures`、`note`、`by` |
| UC-AA-06 跟踪干预记录 | GET | `/alerts/:alert_id/interventions` | 干预历史与是否超时 |
| UC-AA-07 分析学业趋势 | GET | `/students/:student_id/trend` | 多学期综合成绩趋势 |

## 规则字段

- `metric`：`avg_score`（综合成绩）、`failed_courses`（不及格门数）、`credit_rate`（学分完成率）、`attendance`（出勤率）
- `operator`：`lt` / `lte` / `gt` / `gte`
- `level`：`red` / `orange` / `yellow`

默认规则：R1 综合成绩 < 60 红色、R2 不及格 ≥ 2 门橙色、R3 出勤率 < 80% 黄色、R4 学分完成率 < 60% 橙色。

## 业务规则（落地自 UC-AA-05 / 3.3 关键用例规约）

1. 预警已被他人认领时再次认领会返回 409（越权不可认领）
2. 未填写 `measures` 时不允许 `close`，返回 400
3. 标记 `not_applicable`（转专业、休学）必须填写 `note`
4. 关闭后不可修改历史，重复处置返回 409
5. `POST /alerts/generate` 幂等：同一学生同一规则存在未关闭预警时不重复生成
6. 未处置超过 48 小时标记 `overdue`，用于 UC-AA-06 的提醒

## 数据说明

当前用内存种子数据（12 名学生、3 个班级、3 个学期趋势），便于先打通接口。后续可替换为：

- 学生与成绩：M1/M2 的主数据与指标值
- 指标口径：M2 的指标口径配置（避免在 M4 里写死阈值）
- 权限范围：M6 的角色与数据范围校验（当前认领只做「先到先得」）

## 验证

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/platform_server 8080
```

打开 <http://127.0.0.1:8080/>，在测试台里点按钮即可逐个调用上面的接口。
