# 高校学生成长与职业发展数据中台

这是供六个模块并行开发的 C++ 工程骨架。当前只提供本地 HTTP 服务、模块注册、路由接口、浏览器测试台和测试；**没有实现业务模块**。需求范围见《实验一报告》。

## 环境与运行

- Windows（Winsock）或 Linux（POSIX socket）
- 支持 C++17 的编译器、CMake 3.21 或更新版本
- Python 3 可选，用于 HTTP 冒烟测试
- SQLite 3.54.1 以源码形式内置在 `third_party/sqlite/`，不需要额外安装数据库服务

Linux：

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/linux-debug/platform_server 8080
```

Windows / VS Code：安装 CMake Tools 与 C/C++ 扩展，打开项目后选择 `windows-vs2026` preset，`F7` 构建、`F5` 运行。命令行等价操作：

```powershell
cmake --preset windows-vs2026
cmake --build --preset windows-vs2026
ctest --preset windows-vs2026
.\build\windows-vs2026\Debug\platform_server.exe 8080
```

打开 <http://127.0.0.1:8080/>。服务只监听本机回环地址；端口参数可改，省略时为 8080。测试台列出已经注册的模块和路由，并允许填写方法、路径、请求头和请求体，查看实际 HTTP 响应。它是开发工具，不是最终产品前端。

现在可用的公共接口：

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| GET | `/api/health` | 检查服务是否启动 |
| GET | `/api/modules` | 列出六个模块及各自已注册的路由 |

尚未开发的业务地址返回 404，不提供模拟成功结果。

## 模块边界

| 模块 | 目录 | 路由前缀 |
| --- | --- | --- |
| 数据采集与接入 | `src/modules/data_ingestion/` | `/api/modules/data-ingestion/` |
| 数据治理与标准化 | `src/modules/data_governance/` | `/api/modules/data-governance/` |
| 学生画像 | `src/modules/student_profile/` | `/api/modules/student-profile/` |
| 学业分析与预警 | `src/modules/academic_alerts/` | `/api/modules/academic-alerts/` |
| 职业发展推荐 | `src/modules/career_recommendation/` | `/api/modules/career-recommendation/` |
| 数据服务与权限管理 | `src/modules/data_services/` | `/api/modules/data-services/` |

每个 `module.cpp` 是本模块的注册入口。新增源文件时，把它列入 `CMakeLists.txt` 的 `platform_core`。公共请求、响应和路由接口在 `include/platform/app.hpp`，注册入口声明在 `include/platform/modules.hpp`，组合顺序在 `src/app.cpp`。前端资源在 `web/`；组员的业务代码主要写在各自模块目录中。

## 数据库与共享工具

- `include/platform/json.hpp`：轻量 JSON 解析与输出，模块用它处理请求体和响应体。
- `include/platform/db.hpp`：SQLite 薄封装，提供 `exec`、`query`、参数绑定和事务。
- 默认数据库文件为 `data/platform.db`，可用环境变量 `PLATFORM_DB_PATH` 覆盖；数据库文件不提交。
- 表名按模块加前缀（数据接入 `ing_`、学生画像 `sp_`），各模块在自己的仓储构造函数里执行 `CREATE TABLE IF NOT EXISTS`。
- 数据采集与接入、学生画像两个模块的模型、仓储和表结构已经搭好，详见 [`docs/模块框架说明.md`](docs/模块框架说明.md)；业务路由尚未注册。

## 添加一个接口

以学生画像模块为例，在它的 `register_student_profile` 函数内登记路由：

```cpp
app.add_module_route("student-profile", "GET", "/:student_id",
    [](const platform::Request& request) {
        const auto& id = request.path_params.at("student_id");
        return platform::Response{200, "text/plain; charset=utf-8", id};
    });
```

随后请求 `GET /api/modules/student-profile/123`，测试台会自动列出该路由。动态路径段写作 `:参数名`。`Request` 提供 `method`、`path`、小写键名的 `headers`、原始 `body`、原始 `query` 和 `path_params`；`Response` 提供状态码、内容类型、响应体和可选响应头。路由注册遇到未知模块或重复的“方法 + 路径”会报错。

新增业务接口时，同时添加能验证其行为的测试。`tests/app_test.cpp` 演示了直接调用应用层的测试；`tests/http_smoke.py` 验证实际 HTTP 边界。建议先写一个可运行的接口和对应测试，再继续下一个接口。

## 当前边界

这个轻量服务只用于本地联调：单线程、每个连接只处理一个请求、请求体上限 1 MiB，不支持分块传输、TLS 或生产部署。路由采用固定路径和 `:参数名`；查询串与请求体仅保留原文，具体格式解析由模块自行决定。鉴权、持久化、业务实体和六个模块的功能都留待各模块分支实现。涉及通用接口变更时，先在主分支协商，避免六个分支分别修改核心文件。
