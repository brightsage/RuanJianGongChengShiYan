# 运行数据目录

服务启动后默认在这里生成 `platform.db`（SQLite）。数据库文件、WAL 和 SHM 文件已在 `.gitignore` 中忽略，不要提交。

可用环境变量 `PLATFORM_DB_PATH` 指定其他位置，例如：

```powershell
$env:PLATFORM_DB_PATH = "D:\tmp\platform.db"
```
