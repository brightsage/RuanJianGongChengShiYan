# 数据治理与标准化模块

## 模块功能
高校学生成长与职业发展数据中台 - 数据治理模块，负责数据清洗、主数据归并、指标计算和质量监控。

## 接口列表
| 接口路径 | 方法 | 功能 |
|---|---|---|
| /quality-rules | GET | 获取质量规则列表 |
| /quality-rules | POST | 新增质量规则 |
| /student-master/:student_id | GET | 查询学生主数据 |
| /recalculate | POST | 触发指标重算 |
| /quality-report | GET | 获取数据质量报告 |

## 目录结构
```
├── include/data_governance/   # 头文件目录
│   ├── types.h               # 公共数据结构
│   ├── response_utils.h      # 响应工具函数
│   ├── quality_rules.h       # 质量规则模块
│   ├── student_master.h       # 学生主数据模块
│   ├── metrics.h             # 指标计算模块
│   └── quality_report.h      # 质量报告模块
├── src/                       # 源文件目录
│   ├── module.cpp            # 模块入口注册
│   ├── types.cpp             # 数据结构实现
│   ├── quality_rules.cpp     # 质量规则实现
│   ├── student_master.cpp    # 主数据实现
│   ├── metrics.cpp           # 指标实现
│   └── quality_report.cpp    # 质量报告实现
├── tests/                     # 测试用例
│   └── test_data_governance.cpp
├── web/                       # 前端测试页面
│   └── index.html
├── CMakeLists.txt             # 编译配置
└── README.md
```

## 使用方式
1. 将include和src目录下的文件复制到工程对应目录
2. 在工程模块注册处调用 `register_data_governance(app)`
3. 编译后访问 `/api/modules/data-governance/` 前缀即可使用所有接口
