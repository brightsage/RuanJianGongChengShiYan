# SQLite 3.54.1（amalgamation）

- 来源：<https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip>
- 文件：`sqlite3.c`、`sqlite3.h`
- 许可：SQLite 为公有领域（public domain），可随项目一起分发。

选择 SQLite 的原因：

- 单文件嵌入式数据库，不需要安装数据库服务，适合课程项目和本地演示；
- 提供完整 SQL、事务、索引和约束，后续做多表统计、画像快照查询不需要换技术栈；
- 官方 amalgamation 只有两个文件，直接由 CMake 编译进 `platform_core`，五个模块合并时不需要各自装环境。

更新版本时，替换本目录下的 `sqlite3.c`/`sqlite3.h`，重新构建并运行 `platform_storage_tests` 即可。
