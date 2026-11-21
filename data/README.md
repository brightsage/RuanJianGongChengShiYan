# 运行数据目录

项目已经切换到 MySQL，不再在这里生成 `platform.db`。该目录只保留为旧版本兼容占位，数据库文件不会提交。

默认连接：

```text
mysql://platform:PlatformPass123!@127.0.0.1:3307/student_platform
```

可用 `PLATFORM_DB_URL` 或 `PLATFORM_DB_HOST/PORT/USER/PASSWORD/NAME` 覆盖。测试使用 `PLATFORM_TEST_DB=1` 自动创建 `_test_<pid>_<timestamp>` 独立数据库（每个测试进程一个库）。
