# 实验一 · 需求分析与迭代规划（选题 3：高校学生成长与职业发展数据中台）

本目录为实验一的全部交付物，**正稿为 `docs/需求规格说明书.docx`**（同时提供 PDF 预览版，便于快速翻阅）。

## 交付物清单

| 路径 | 说明 |
|---|---|
| `docs/需求规格说明书.docx` | 正稿。含封面、自动目录、正文 3 章、附录 A/B，共 41 页、18 张表、16 张图 |
| `docs/需求规格说明书.pdf` | 预览版（LibreOffice 转换，分页与 Word 略有差异，以 docx 为准） |
| `diagrams/png/` | 全部图形的成图（用户故事地图 ×2、用例图 ×6、活动图 ×6、域模型 ×2） |
| `diagrams/src/` | 活动图与域模型的 PlantUML 源文件（可直接改文本重渲染） |
| `tools/reqdata.py` | **需求基线的单一数据源**：模块、角色、用例、用户故事、迭代、非功能需求、域模型类 |
| `tools/build_usecase.py` | 6 张用例图的布局配置（改这里即可改用例图） |
| `tools/render_usecase.py` | 用例图确定性布局渲染器（PIL） |
| `tools/storymap.py` | 用户故事地图渲染器（读 `reqdata.py` 的故事数据） |
| `tools/gen_puml.py` | 生成活动图、域模型的 `.puml` |
| `tools/build_docx.py` | 由数据 + 图生成 Word 文档（含表格、题注、目录域、分节页码） |

图表与表格数据同源，改 `reqdata.py` 后重跑脚本即可整体同步，不会出现“图改了表没改”的不一致。

## 重新生成步骤

```powershell
$py = "C:\Users\brigh\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe"
cd 实验一\tools

& $py storymap.py        # 用户故事地图 -> diagrams/png
& $py build_usecase.py   # 用例图       -> diagrams/png
& $py gen_puml.py        # 生成 .puml
& java -jar ..\..\.tools\plantuml.jar -charset UTF-8 -tpng -o ..\diagrams\png ..\diagrams\src\*.puml
& $py build_docx.py      # 生成 docx
```

生成 docx 后，用 Word 打开并右键目录 →“更新域”，即可刷新页码（目录页码是按 Word 分页计算的）。

## 提交前需要替换的内容

1. 封面上的 **班级、组长、小组成员、指导教师、完成日期**（现为下划线占位符）；
2. 若小组人数不是 5 人、周期不是 8 周/4 个 Sprint，改 `reqdata.py` 中的 `SPRINTS` 与 `STORIES` 的迭代归属，再重跑脚本；
3. 报告中“技术栈”默认按 Spring Boot + Vue 描述的部分（主要在背景与范围一节），若小组选型不同请同步修改。

## 与实验要求的对应关系

| 实验一要求 | 报告位置 |
|---|---|
| 调研分析、确定系统需求 | 1.1 项目背景与目标、1.1.3 范围与边界、1.1.4 干系人 |
| 需求拆解为可管理的用户故事 | 1.2.1 拆解方法（故事卡 + INVEST + 故事点 + 验收标准）、附录表A-1 |
| 用户故事地图组织与管理 | 1.2.2（图1-1、图1-2 + 三条使用规则）、1.2.6 变更管理 |
| 迭代规划 | 1.2.4 + 附录表A-2、1.2.5 迭代节奏与 Git 规范 |
| 按功能模块绘制用例图、用例表 | 2.3.2～2.3.7（6 张图 + 6 张表）、2.3.1 建模约定 |
| 复杂事件流绘制活动图 | 各模块小节内 6 张活动图 + 2.3.8 四个关键用例规约 |
| UML 建立软件问题模型 | 2.4 数据建模（名词短语法、CRC、域模型、分析类清单） |
| 非功能需求 | 第 3 章 + 验证方式说明 |
